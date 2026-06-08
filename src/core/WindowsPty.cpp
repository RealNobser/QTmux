#include "Pty.h"

#if defined(Q_OS_WIN)

// ConPTY-Implementierung (Windows 10 1809+ / Build 17763).
//
// Architektur (analog zum forkpty-Pfad in UnixPty.cpp):
//   1. CreatePipe() für die Eingabe- und Ausgabe-Pipe der Pseudo-Konsole.
//   2. CreatePseudoConsole(size, hInputRead, hOutputWrite, 0, &hPC).
//   3. STARTUPINFOEX mit PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE (hängt den
//      Kindprozess an die Pseudo-Konsole; KEIN bInheritHandles/STARTF_USESTDHANDLES).
//   4. CreateProcessW() der Shell.
//   5. Lesen der Ausgabe-Pipe in einem dedizierten Reader-Thread (blockierendes
//      ReadFile). QSocketNotifier funktioniert auf Windows nicht für Pipes; statt
//      Overlapped I/O ist ein eigener Thread + queued Marshaling die robuste Lösung.
//   6. resize -> ResizePseudoConsole(); terminate -> Prozessbaum beenden +
//      ClosePseudoConsole() (löst EOF auf der Ausgabe-Pipe aus -> Reader endet).

#include "ProcessInfo.h"

#include <windows.h>

#include <QMap>
#include <QMetaObject>
#include <QProcessEnvironment>

#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

namespace qtmux {

struct Pty::Private {
    HPCON hPC = nullptr;
    HANDLE hInputWrite = nullptr;   // wir schreiben hierhin -> Pseudo-Konsole
    HANDLE hOutputRead = nullptr;   // wir lesen hier -> Ausgabe der Pseudo-Konsole
    PROCESS_INFORMATION pi {};
    LPPROC_THREAD_ATTRIBUTE_LIST attrList = nullptr;
    std::thread reader;
    std::thread waiter;             // wartet auf das Prozessende der Shell
    std::atomic<bool> stopping{false};
};

// --- Hilfsfunktionen --------------------------------------------------------

// Quoting eines einzelnen Arguments nach den CommandLineToArgvW-Regeln.
static QString quoteWinArg(const QString &arg) {
    if (!arg.isEmpty()
        && !arg.contains(QLatin1Char(' '))
        && !arg.contains(QLatin1Char('\t'))
        && !arg.contains(QLatin1Char('"'))) {
        return arg;
    }
    QString result = QStringLiteral("\"");
    int backslashes = 0;
    for (const QChar c : arg) {
        if (c == QLatin1Char('\\')) {
            ++backslashes;
        } else if (c == QLatin1Char('"')) {
            result.append(QString(backslashes * 2 + 1, QLatin1Char('\\')));
            result.append(c);
            backslashes = 0;
        } else {
            if (backslashes) { result.append(QString(backslashes, QLatin1Char('\\'))); backslashes = 0; }
            result.append(c);
        }
    }
    if (backslashes) result.append(QString(backslashes * 2, QLatin1Char('\\')));
    result.append(QLatin1Char('"'));
    return result;
}

static QString buildCommandLine(const QString &program, const QStringList &args) {
    QString cmd = quoteWinArg(program);
    for (const QString &a : args) cmd += QLatin1Char(' ') + quoteWinArg(a);
    return cmd;
}

// Erstellt einen UTF-16-Umgebungsblock: aktuelle Umgebung + Overrides ("KEY=VALUE"),
// alphabetisch (case-insensitive) sortiert, doppelt null-terminiert.
static std::vector<wchar_t> buildEnvironmentBlock(const QStringList &overrides) {
    QMap<QString, QString> envMap;   // QMap sortiert case-sensitive; wir normalisieren den Key
    QMap<QString, QString> keyCase;  // normalisierter Key -> Original-Schreibweise

    auto put = [&](const QString &key, const QString &value) {
        const QString norm = key.toUpper();
        envMap.insert(norm, value);
        keyCase.insert(norm, key);
    };

    for (const QString &kv : QProcessEnvironment::systemEnvironment().toStringList()) {
        const int eq = kv.indexOf(QLatin1Char('='));
        if (eq > 0) put(kv.left(eq), kv.mid(eq + 1));
    }
    for (const QString &kv : overrides) {
        const int eq = kv.indexOf(QLatin1Char('='));
        if (eq > 0) put(kv.left(eq), kv.mid(eq + 1));
    }

    std::vector<wchar_t> block;
    for (auto it = envMap.constBegin(); it != envMap.constEnd(); ++it) {
        const QString entry = keyCase.value(it.key()) + QLatin1Char('=') + it.value();
        const std::wstring w = entry.toStdWString();
        block.insert(block.end(), w.begin(), w.end());
        block.push_back(L'\0');
    }
    block.push_back(L'\0');   // abschließendes Null des Blocks
    return block;
}

// --- Pty --------------------------------------------------------------------

Pty::Pty(QObject *parent) : QObject(parent), d(new Private) {}

Pty::~Pty() {
    terminate();
    delete d;
}

bool Pty::start(const QString &program, const QStringList &args,
                int cols, int rows, const QStringList &env,
                const QString &workingDir) {
    if (m_running) {
        m_lastError = QStringLiteral("PTY läuft bereits");
        return false;
    }

    HANDLE inRead = nullptr, inWrite = nullptr;
    HANDLE outRead = nullptr, outWrite = nullptr;

    if (!CreatePipe(&inRead, &inWrite, nullptr, 0)) {
        m_lastError = QStringLiteral("CreatePipe (Eingabe) fehlgeschlagen");
        return false;
    }
    if (!CreatePipe(&outRead, &outWrite, nullptr, 0)) {
        CloseHandle(inRead); CloseHandle(inWrite);
        m_lastError = QStringLiteral("CreatePipe (Ausgabe) fehlgeschlagen");
        return false;
    }

    const COORD size {
        static_cast<SHORT>(cols > 0 ? cols : 80),
        static_cast<SHORT>(rows > 0 ? rows : 24)
    };
    HRESULT hr = CreatePseudoConsole(size, inRead, outWrite, 0, &d->hPC);

    // Unsere Kopien der an die Pseudo-Konsole übergebenen Enden werden nicht mehr
    // gebraucht (CreatePseudoConsole hat sie dupliziert).
    CloseHandle(inRead);
    CloseHandle(outWrite);

    if (FAILED(hr)) {
        CloseHandle(inWrite); CloseHandle(outRead);
        d->hPC = nullptr;
        m_lastError = QStringLiteral("CreatePseudoConsole fehlgeschlagen (HRESULT 0x%1)")
                          .arg(static_cast<quint32>(hr), 0, 16);
        return false;
    }

    d->hInputWrite = inWrite;
    d->hOutputRead = outRead;

    // Attributliste mit dem Pseudo-Konsolen-Handle aufbauen.
    SIZE_T attrSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);
    d->attrList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
        HeapAlloc(GetProcessHeap(), 0, attrSize));
    if (!d->attrList
        || !InitializeProcThreadAttributeList(d->attrList, 1, 0, &attrSize)
        || !UpdateProcThreadAttribute(d->attrList, 0,
                                      PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                      d->hPC, sizeof(d->hPC), nullptr, nullptr)) {
        m_lastError = QStringLiteral("ProcThreadAttributeList-Aufbau fehlgeschlagen");
        if (d->attrList) { HeapFree(GetProcessHeap(), 0, d->attrList); d->attrList = nullptr; }
        ClosePseudoConsole(d->hPC); d->hPC = nullptr;
        CloseHandle(d->hInputWrite); d->hInputWrite = nullptr;
        CloseHandle(d->hOutputRead); d->hOutputRead = nullptr;
        return false;
    }

    STARTUPINFOEXW si {};
    si.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    si.lpAttributeList = d->attrList;

    QString cmdLineStr = buildCommandLine(program, args);
    std::wstring cmdLineW = cmdLineStr.toStdWString();
    std::vector<wchar_t> cmdLine(cmdLineW.begin(), cmdLineW.end());
    cmdLine.push_back(L'\0');

    std::vector<wchar_t> envBlock = buildEnvironmentBlock(env);
    const std::wstring wWorkingDir = workingDir.toStdWString();

    const BOOL ok = CreateProcessW(
        nullptr,                       // Anwendung wird aus der Kommandozeile aufgelöst (PATH)
        cmdLine.data(),
        nullptr, nullptr,
        FALSE,                         // KEINE Handle-Vererbung (Pseudo-Konsole via Attribut)
        EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
        envBlock.data(),
        workingDir.isEmpty() ? nullptr : wWorkingDir.c_str(),
        &si.StartupInfo,
        &d->pi);

    if (!ok) {
        m_lastError = QStringLiteral("CreateProcessW fehlgeschlagen (GetLastError %1)")
                          .arg(static_cast<quint32>(GetLastError()));
        DeleteProcThreadAttributeList(d->attrList);
        HeapFree(GetProcessHeap(), 0, d->attrList); d->attrList = nullptr;
        ClosePseudoConsole(d->hPC); d->hPC = nullptr;
        CloseHandle(d->hInputWrite); d->hInputWrite = nullptr;
        CloseHandle(d->hOutputRead); d->hOutputRead = nullptr;
        return false;
    }

    m_pid = static_cast<qint64>(d->pi.dwProcessId);
    m_running = true;
    d->stopping = false;

    // Reader-Thread: blockierendes ReadFile, Daten per queued Connection an den
    // GUI-Thread marshalen.
    d->reader = std::thread([this]() {
        char buf[8192];
        for (;;) {
            DWORD n = 0;
            const BOOL r = ReadFile(d->hOutputRead, buf, sizeof(buf), &n, nullptr);
            if (!r || n == 0) break;
            QByteArray chunk(buf, static_cast<int>(n));
            QMetaObject::invokeMethod(this, [this, chunk]() {
                emit readyRead(chunk);
            }, Qt::QueuedConnection);
        }
    });

    // Waiter-Thread: erkennt das Ende der Shell. Anders als beim Unix-PTY (EOF auf
    // dem Master-FD) schließt conhost die Ausgabe-Pipe NICHT, wenn sich das Kind
    // selbst beendet (z. B. `exit`) — der Reader bekäme also kein EOF. Daher warten
    // wir direkt auf das Prozess-Handle und lösen dann das geordnete Ende aus.
    d->waiter = std::thread([this]() {
        WaitForSingleObject(d->pi.hProcess, INFINITE);
        if (d->stopping) return;   // terminate() läuft bereits
        QMetaObject::invokeMethod(this, [this]() {
            if (m_running) terminate();
        }, Qt::QueuedConnection);
    });

    return true;
}

qint64 Pty::write(const QByteArray &data) {
    if (!m_running || !d->hInputWrite) return -1;
    DWORD written = 0;
    if (!WriteFile(d->hInputWrite, data.constData(),
                   static_cast<DWORD>(data.size()), &written, nullptr))
        return -1;
    return static_cast<qint64>(written);
}

void Pty::resize(int cols, int rows) {
    if (!m_running || !d->hPC) return;
    const COORD size {
        static_cast<SHORT>(cols > 0 ? cols : 80),
        static_cast<SHORT>(rows > 0 ? rows : 24)
    };
    ResizePseudoConsole(d->hPC, size);
}

void Pty::terminate() {
    if (!m_running) return;
    m_running = false;
    d->stopping = true;

    const qint64 pid = m_pid;

    // Nachfahren erfassen, solange der Baum intakt ist (analog zum Unix-Pfad),
    // damit auch Enkelprozesse mit beendet werden.
    QList<qint64> tree;
    if (pid > 0) tree = procinfo::descendantPids(pid);

    // WICHTIG: Pseudo-Konsole ZUERST schließen. Das beendet die angehängten
    // Clients geordnet und lässt conhost die Ausgabe-Pipe schließen -> EOF, der
    // Reader-Thread kehrt aus ReadFile zurück. Beendet man den Prozess vorher
    // hart, hängt ClosePseudoConsole/der Reader (conhost gibt kein EOF) — das
    // war die Ursache mehrsekündiger Hänger beim Beenden.
    if (d->hPC) { ClosePseudoConsole(d->hPC); d->hPC = nullptr; }

    // Wurzelprozess hart beenden (falls er nicht ohnehin schon endet).
    if (d->pi.hProcess) TerminateProcess(d->pi.hProcess, 1);

    // Sicherheitsnetz: ein evtl. noch in ReadFile blockierender Reader wird
    // entkoppelt, dann beigetreten.
    if (d->reader.joinable()) {
        CancelSynchronousIo(d->reader.native_handle());
        d->reader.join();
    }
    // Waiter beitreten: WaitForSingleObject kehrt nach TerminateProcess (bzw. dem
    // bereits erfolgten Prozessende) zurück. d->stopping verhindert Rückruf-Schleifen.
    if (d->waiter.joinable()) d->waiter.join();

    // Überlebende Nachfahren hart beenden.
    for (const qint64 p : tree) {
        if (p <= 0 || p == pid) continue;
        if (HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(p))) {
            TerminateProcess(h, 1);
            CloseHandle(h);
        }
    }

    DWORD exitCode = 0;
    if (d->pi.hProcess) GetExitCodeProcess(d->pi.hProcess, &exitCode);

    if (d->hInputWrite) { CloseHandle(d->hInputWrite); d->hInputWrite = nullptr; }
    if (d->hOutputRead) { CloseHandle(d->hOutputRead); d->hOutputRead = nullptr; }
    if (d->pi.hThread)  { CloseHandle(d->pi.hThread);  d->pi.hThread = nullptr; }
    if (d->pi.hProcess) { CloseHandle(d->pi.hProcess); d->pi.hProcess = nullptr; }
    if (d->attrList) {
        DeleteProcThreadAttributeList(d->attrList);
        HeapFree(GetProcessHeap(), 0, d->attrList);
        d->attrList = nullptr;
    }

    m_pid = 0;
    emit finished(static_cast<int>(exitCode));
}

QString Pty::currentWorkingDirectory() const {
    // Das Arbeitsverzeichnis eines fremden Prozesses lässt sich auf Windows nur
    // über das PEB (NtQueryInformationProcess + ReadProcessMemory) ermitteln —
    // bewusst zurückgestellt. Folge: Beim Neustart startet die Shell im Home,
    // nicht im zuletzt genutzten Verzeichnis.
    return {};
}

} // namespace qtmux

#endif // Q_OS_WIN

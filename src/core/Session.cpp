#include "Session.h"
#include "VtScreen.h"
#include "AgentRegistry.h"
#include "ColorScheme.h"

#include <QCoreApplication>
#include <QHash>

namespace qtmux {

namespace {

// Macht aus einem rohen Fenstertitel/Programmpfad einen lesbaren Namen:
//   - bekannte KI-Agenten (claude, codex, …)        -> Anzeigename aus AgentRegistry
//   - übliche Shells (powershell/cmd/bash/zsh/…)     -> freundlicher Name
//   - alles andere (z. B. "user@host: ~/projekt")    -> unverändert
// Auf Windows liefert conhost als Titel den vollen exe-Pfad — den wandeln wir so
// z. B. in "PowerShell" bzw. "Eingabeaufforderung".
QString prettifyTitle(const QString &raw) {
    if (raw.isEmpty()) return raw;

    // Basisnamen ohne Pfad ermitteln (Unix '/' wie Windows '\').
    const int slash = qMax(raw.lastIndexOf(QLatin1Char('/')),
                           raw.lastIndexOf(QLatin1Char('\\')));
    const QString base = slash >= 0 ? raw.mid(slash + 1) : raw;

    // Bekannte Agenten über die zentrale Registry erkennen (deckt auch
    // "claude.exe", Pfade und Groß-/Kleinschreibung ab).
    if (const AgentInfo *a = AgentRegistry::detect(base)) return a->displayName;

    // Für den Shell-Abgleich die .exe/.cmd/.bat-Endung strippen.
    QString name = base;
    for (const QString &suf : {QStringLiteral(".exe"), QStringLiteral(".cmd"),
                               QStringLiteral(".bat")}) {
        if (name.endsWith(suf, Qt::CaseInsensitive)) { name.chop(suf.size()); break; }
    }
    const QString lower = name.toLower();

    // "Eingabeaufforderung" ist lokalisierbar (Kontext "Shells"); pro Aufruf
    // übersetzt, damit ein Laufzeit-Sprachwechsel beim nächsten Titel greift.
    if (lower == QLatin1String("cmd"))
        return QCoreApplication::translate("Shells", "Eingabeaufforderung");

    // Eigennamen — sprachunabhängig, daher nicht übersetzt.
    static const QHash<QString, QString> kShells = {
        {QStringLiteral("powershell"), QStringLiteral("PowerShell")},
        {QStringLiteral("pwsh"),       QStringLiteral("PowerShell 7")},
        {QStringLiteral("bash"),       QStringLiteral("Bash")},
        {QStringLiteral("zsh"),        QStringLiteral("Zsh")},
        {QStringLiteral("fish"),       QStringLiteral("fish")},
        {QStringLiteral("sh"),         QStringLiteral("sh")},
        {QStringLiteral("wsl"),        QStringLiteral("WSL")},
    };
    const auto it = kShells.constFind(lower);
    if (it != kShells.constEnd()) return it.value();

    return raw;   // unbekannt -> Originaltitel beibehalten
}

} // namespace

Session::Session(QObject *parent) : QObject(parent) {}
Session::~Session() = default;

int Session::nextId() {
    static int counter = 0;
    return ++counter;
}

QString Session::screenText() const {
    return m_screen ? m_screen->screenText() : QString();
}

void Session::attachBackend(ITerminalBackend *backend, Type type, int cols, int rows) {
    m_type = type;
    m_cols = cols;
    m_rows = rows;
    // Besitz liegt beim unique_ptr — KEIN setParent(this) (würde Double-Free verursachen).
    m_backend.reset(backend);

    m_screen = std::make_unique<VtScreen>(rows, cols);

    connect(m_backend.get(), &ITerminalBackend::dataReceived,
            m_screen.get(), &VtScreen::inputWrite);
    connect(m_screen.get(), &VtScreen::outputToPty,
            m_backend.get(), &ITerminalBackend::write);
    connect(m_screen.get(), &VtScreen::titleChanged, this, &Session::setTitle);
    connect(m_screen.get(), &VtScreen::bell, this, &Session::onBell);
    connect(m_screen.get(), &VtScreen::notify, this, &Session::onNotify);
    connect(m_screen.get(), &VtScreen::promptMarker, this, &Session::onPromptMarker);

    // Aktuelles Farbschema anwenden und bei Wechsel automatisch neu setzen.
    auto *schemes = ColorSchemeRegistry::instance();
    m_screen->applyColorScheme(schemes->currentScheme());
    connect(schemes, &ColorSchemeRegistry::changed, m_screen.get(), [this, schemes]() {
        if (m_screen) m_screen->applyColorScheme(schemes->currentScheme());
    });

    // Zustand aus dem Signal-Argument nehmen (NICHT m_backend dereferenzieren — das
    // Signal kann während der Zerstörung des Backends feuern).
    connect(m_backend.get(), &ITerminalBackend::stateChanged, this,
            [this](BackendState st) {
                emit stateChanged();
                if (st == BackendState::Closed) setActivity(Activity::Closed);
            });
}

void Session::setActive(bool active) {
    m_active = active;
    if (active && m_needsAttention) {
        m_needsAttention = false;
        emit attentionChanged();
    }
}

void Session::setMcpController(bool on) {
    if (on == m_mcpController) return;
    m_mcpController = on;
    emit mcpControllerChanged();
}

void Session::shutdown() {
    if (m_backend) m_backend->terminate();
}

void Session::raiseAttention() {
    if (!m_active && !m_needsAttention) {
        m_needsAttention = true;
        emit attentionChanged();
    }
}

void Session::setActivity(Activity a) {
    if (a == m_activity) return;
    m_activity = a;
    emit activityChanged();
}

void Session::onBell() {
    emit bell();
    raiseAttention();   // Bell einer nicht-fokussierten Session = Aufmerksamkeit
}

void Session::onNotify(const QString &text) {
    // OSC 9/777: Notification-Text merken (Sidebar) und Aufmerksamkeit anfordern.
    m_lastNotification = text;
    emit notificationChanged();
    raiseAttention();
}

void Session::onPromptMarker(char kind, int exitCode) {
    // OSC 133 (FinalTerm/Shell-Integration): Befehls-Lebenszyklus verfolgen.
    switch (kind) {
    case 'C':                       // Befehl beginnt Ausgabe
        m_commandRunning = true;
        setActivity(Activity::Running);
        break;
    case 'D':                       // Befehl beendet (exitCode)
        setActivity(exitCode > 0 ? Activity::Error : Activity::Running);
        if (m_commandRunning) {     // echtes Kommando lief -> bei Inaktivität melden
            m_commandRunning = false;
            raiseAttention();
        }
        break;
    case 'A':                       // neue Prompt
    case 'B':                       // Prompt bereit für Eingabe
        if (m_activity == Activity::Error) setActivity(Activity::Running);
        break;
    default:
        break;
    }
}

void Session::setTitle(const QString &t) {
    const QString pretty = prettifyTitle(t);
    if (pretty.isEmpty() || pretty == m_title) return;
    m_title = pretty;
    emit titleChanged(m_title);
}

void Session::start(int cols, int rows) {
    if (!m_backend) return;
    m_cols = cols;
    m_rows = rows;
    m_screen->setSize(rows, cols);
    m_backend->start(cols, rows);
}

void Session::write(const QByteArray &data) {
    observeInput(data);
    if (m_backend) m_backend->write(data);
}

void Session::observeInput(const QByteArray &data) {
    // Rekonstruiert die getippte Zeile zeichenweise und erkennt beim Enter
    // ein bekanntes Agenten-Kommando (z. B. "agy" -> AntiGravity).
    for (char ch : data) {
        if (ch == '\r' || ch == '\n') {
            if (const AgentInfo *agent = AgentRegistry::detect(m_inputLine)) {
                if (m_agentId != agent->id) {
                    m_agentId = agent->id;
                    emit agentChanged();
                }
                m_titleFromAgent = true;
                setTitle(agent->displayName);
            }
            m_inputLine.clear();
        } else if (ch == '\x7f' || ch == '\b') {  // Backspace
            m_inputLine.chop(1);
        } else if (static_cast<unsigned char>(ch) >= 0x20) {
            m_inputLine.append(QChar::fromLatin1(ch));
        } else {
            // Steuerzeichen (Ctrl-C, Pfeiltasten-Escapes …) verwerfen die Zeilenannahme.
            m_inputLine.clear();
        }
    }
}

void Session::resize(int cols, int rows) {
    if (cols == m_cols && rows == m_rows) return;
    m_cols = cols;
    m_rows = rows;
    if (m_screen) m_screen->setSize(rows, cols);
    if (m_backend) m_backend->resize(cols, rows);
}

} // namespace qtmux

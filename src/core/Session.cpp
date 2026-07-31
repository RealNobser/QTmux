#include "Session.h"
#include "VtScreen.h"
#include "AgentRegistry.h"
#include "AgentEventHub.h"
#include "ColorScheme.h"

#include <QCoreApplication>
#include <QHash>
#include <QTimer>
#include <QStringList>
#include <QRegularExpression>

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

    // Clink-Shell (QTMUX-25): das Programm ist die Kommandozeile
    // `cmd.exe /k "<clink.bat>" inject --quiet`. Sie als solche benennen, statt aus
    // ihr einen Basisnamen zu schnitzen. (Greift nur auf den anfänglichen, aus dem
    // Programm abgeleiteten Titel; ein späterer OSC-Titel der Shell ersetzt ihn.)
    if (raw.contains(QLatin1String("clink"), Qt::CaseInsensitive)
        && raw.contains(QLatin1String("cmd"), Qt::CaseInsensitive))
        return QCoreApplication::translate("Shells", "Eingabeaufforderung (Clink)");

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

QString effectiveWorkingDirectory(const QString &reported, bool reportedIsLocal,
                                  const QString &polled) {
    if (reportedIsLocal && !reported.isEmpty()) return reported;
    return polled;
}

Session::Session(QObject *parent) : QObject(parent) {}
Session::~Session() = default;

int Session::nextId() {
    static int counter = 0;
    return ++counter;
}

QString Session::screenText() const {
    return m_screen ? m_screen->screenText() : QString();
}

QString Session::scrollbackText() const {
    return m_screen ? m_screen->scrollbackText() : QString();
}

void Session::attachBackend(ITerminalBackend *backend, Type type, int cols, int rows) {
    const bool typeChanged = (type != m_type);
    m_type = type;
    m_cols = cols;
    m_rows = rows;
    // Besitz liegt beim unique_ptr — KEIN setParent(this) (würde Double-Free verursachen).
    m_backend.reset(backend);

    m_screen = std::make_unique<VtScreen>(rows, cols);

    connect(m_backend.get(), &ITerminalBackend::dataReceived,
            m_screen.get(), &VtScreen::inputWrite);
    // Beim ersten Output den Fallback-Timer fürs Login-Script bewaffnen (greift, wenn
    // keine OSC-133-Shell-Integration vorhanden ist) und den Output auf eine SSH-
    // Passwort-Eingabeaufforderung hin abtasten (Vault-Auto-Fill).
    connect(m_backend.get(), &ITerminalBackend::dataReceived, this,
            [this](const QByteArray &data) { scanForPasswordPrompt(data); armLoginScript(); });
    connect(m_screen.get(), &VtScreen::outputToPty,
            m_backend.get(), &ITerminalBackend::write);
    connect(m_screen.get(), &VtScreen::titleChanged, this, &Session::setTitle);
    connect(m_screen.get(), &VtScreen::bell, this, &Session::onBell);
    connect(m_screen.get(), &VtScreen::notify, this, &Session::onNotify);
    connect(m_screen.get(), &VtScreen::progress, this, &Session::onProgress);
    connect(m_screen.get(), &VtScreen::promptMarker, this, &Session::onPromptMarker);
    connect(m_screen.get(), &VtScreen::agentEvent, this, &Session::onAgentEvent);
    connect(m_screen.get(), &VtScreen::workingDirectoryReported,
            this, &Session::onWorkingDirectoryReported);

    // Der Typ steht erst hier fest (nicht im Konstruktor) — die Oberfläche wählt daraus
    // ihr Backend-Icon in der eingeklappten Seitenleiste.
    if (typeChanged) emit backendTypeChanged();

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

void Session::setGroup(const QString &g) {
    const QString t = g.trimmed();
    if (t == m_group) return;
    m_group = t;
    emit groupChanged();
}

QString Session::currentWorkingDirectory() const {
    const QString polled = m_backend ? m_backend->currentWorkingDirectory() : QString();
    if (!m_cwdReported) return polled;
    return effectiveWorkingDirectory(m_reportedDir, m_reportedLocal, polled);
}

void Session::onWorkingDirectoryReported(const QString &path, const QString &host, bool local) {
    // Die Shell hat gemeldet (OSC 7) — ab jetzt ist sie die Quelle, das Polling schweigt.
    // Bewusst OHNE Typprüfung: Genau bei SSH und in Containern ist die Meldung das
    // Einzige, was den Ort überhaupt kennt; ein fremder Host wird dabei vermerkt,
    // damit niemand den Pfad für ein lokales Verzeichnis hält.
    if (path.isEmpty()) return;
    m_cwdReported = true;
    m_reportedLocal = local;
    m_reportedDir = path;
    m_reportedHost = host;
    if (path == m_workingDir) return;
    m_workingDir = path;
    emit workingDirectoryChanged();
}

void Session::refreshWorkingDirectory() {
    // Meldet die Shell selbst (OSC 7), ist hier nichts zu tun — der gepollte Wert wäre
    // schlechter (PowerShell) oder schlicht der falsche Rechner (SSH/Container) und
    // würde die Meldung bei jedem Durchlauf überschreiben.
    if (m_cwdReported) return;
    // Nur Shell-Sessions haben ein sinnvolles lokales Arbeitsverzeichnis. Bei SSH
    // wäre es das CWD des lokalen ssh-Clients (irreführend), bei Seriell/Plugin gibt
    // es keins — daher dort nicht abfragen und leer halten.
    if (m_type != Type::Shell) return;
    const QString dir = currentWorkingDirectory();
    if (dir != m_workingDir) {
        m_workingDir = dir;
        emit workingDirectoryChanged();
    }
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
    m_activitySinceMs = QDateTime::currentMSecsSinceEpoch();
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

void Session::onAgentEvent(const QString &kind, const QString &text) {
    // OSC 777;qtmux-event läuft jetzt durch DENSELBEN Pfad wie das MCP-Werkzeug post_event.
    reportAgentEvent(kind, text);
}

void Session::reportAgentEvent(const QString &kind, const QString &text) {
    // Strukturiertes Agenten-Ereignis (fertig/Frage/Fehler/Info) in den Ereignis-Bus
    // einspeisen — abonnierende Sessions werden per MCP-Long-Poll benachrichtigt. Quelle
    // ist die eigene Session-ID (mit der ein Agent „hier" weiterarbeitet).
    const AgentEventHub::Kind k = AgentEventHub::kindFromString(kind);
    AgentEventHub::instance()->postEvent(m_id, k, text);
    // Lokal als Sidebar-Notiz spiegeln.
    m_lastNotification = text.isEmpty() ? kind : QStringLiteral("%1: %2").arg(kind, text);
    emit notificationChanged();
    // Nur die „Mensch nötig"-Arten wecken die Kachel (Puls); done/info sind FYI und
    // sollen die Sidebar nicht blinken lassen. raiseAttention greift nur, wenn inaktiv.
    if (k == AgentEventHub::Kind::Question || k == AgentEventHub::Kind::Error)
        raiseAttention();
}

void Session::flagAttention(const QString &note) {
    // Explizite Aufmerksamkeits-Anforderung (MCP needs_attention).
    if (!note.isEmpty()) { m_lastNotification = note; emit notificationChanged(); }
    raiseAttention();
}

void Session::clearAttention() {
    if (!m_needsAttention) return;
    m_needsAttention = false;
    emit attentionChanged();
}

void Session::markActivityReported() {
    if (m_activityReported) return;
    m_activityReported = true;
    emit activityChanged();   // Wert evtl. unverändert, seine Aussagekraft aber nicht
}

void Session::requestActivity(const QString &state) {
    const QString s = state.trimmed().toLower();
    // Ab hier meldet die Session ihren Zustand selbst (QTMUX-89).
    if (s == QLatin1String("idle") || s == QLatin1String("busy") || s == QLatin1String("running")
        || s == QLatin1String("waiting") || s == QLatin1String("error"))
        markActivityReported();
    if (s == QLatin1String("idle"))         setActivity(Activity::Idle);
    else if (s == QLatin1String("busy")
          || s == QLatin1String("running")) setActivity(Activity::Running);
    else if (s == QLatin1String("waiting")) setActivity(Activity::Waiting);
    else if (s == QLatin1String("error"))   setActivity(Activity::Error);
    // Unbekannt: bewusst ignorieren (Zustand bleibt unverändert).
}

void Session::onProgress(int state, int value) {
    // OSC 9;4: state 0 = aus, sonst aktiv mit Wert 0..100.
    const bool active = state != 0;
    if (active == m_progressActive && state == m_progressState && value == m_progressValue)
        return;
    m_progressActive = active;
    m_progressState = active ? state : 0;
    m_progressValue = qBound(0, value, 100);
    emit progressChanged();
}

void Session::onPromptMarker(char kind, int exitCode) {
    // OSC 133 (FinalTerm/Shell-Integration): Befehls-Lebenszyklus verfolgen.
    // Ein OSC-133-Marker heißt: Diese Shell hat Integration und meldet ihren Zustand.
    markActivityReported();
    switch (kind) {
    case 'C':                       // Befehl beginnt Ausgabe
        m_commandRunning = true;
        setActivity(Activity::Running);   // beschäftigt (grün)
        break;
    case 'D':                       // Befehl beendet (exitCode)
        // Fehler-Exit -> rot (bleibt am Prompt sichtbar); sonst untätig (dim).
        setActivity(exitCode > 0 ? Activity::Error : Activity::Idle);
        if (m_commandRunning) {     // echtes Kommando lief -> bei Inaktivität melden
            m_commandRunning = false;
            raiseAttention();
        }
        break;
    case 'A':                       // neue Prompt
    case 'B':                       // Prompt bereit für Eingabe
        // Prompt = bereit/untätig (dim). Einen Fehler des letzten Kommandos NICHT löschen —
        // er bleibt am Prompt sichtbar, erst das nächste Kommando (C) setzt ihn zurück.
        if (m_activity != Activity::Error) setActivity(Activity::Idle);
        // Shell-Integration meldet die erste Eingabebereitschaft → exakter Zeitpunkt
        // fürs Login-Script (sicherer als der Fallback-Timer, z. B. nach Passwortabfrage).
        runLoginScript();
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
    const bool changed = (cols != m_cols || rows != m_rows);
    m_cols = cols;
    m_rows = rows;
    m_screen->setSize(rows, cols);
    m_backend->start(cols, rows);
    if (changed) emit sizeChanged();   // QTMUX-120: Statusleiste zeigt das echte Raster
}

void Session::write(const QByteArray &data) {
    observeInput(data);
    if (m_backend) m_backend->write(data);
}

void Session::writeWithEnter(const QByteArray &data, int enterDelayMs) {
    // QTMUX-31: Das abschließende Enter darf NICHT im selben Schreibvorgang wie der
    // Text stehen. TUI-Anwendungen (belegt mit Claude Code) werten einen Byteblock,
    // der in einem Rutsch ankommt, als Einfügevorgang und behandeln ein enthaltenes
    // \r dann als Zeilenumbruch IM Eingabefeld statt als Absenden — der Text blieb
    // stehen, obwohl der Aufruf Erfolg meldete. Zeitlich abgesetzt kommt das Enter
    // als eigener Tastendruck an. Genau das tat auch die bekannte Umgehung
    // (zweiter Aufruf mit leerem Text).
    flushPendingEnter();   // Reihenfolge wahren, falls noch ein Enter aussteht
    if (!data.isEmpty()) write(data);
    if (data.isEmpty() || enterDelayMs <= 0) {
        write(QByteArrayLiteral("\r"));   // nichts zu entzerren
        return;
    }
    m_enterPending = true;
    // An `this` gebunden: stirbt die Session vorher, entfällt das Enter.
    QTimer::singleShot(enterDelayMs, this, [this] { flushPendingEnter(); });
}

void Session::flushPendingEnter() {
    if (!m_enterPending) return;
    m_enterPending = false;
    write(QByteArrayLiteral("\r"));
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
                // Die getippte Zeile mitsamt Argumenten aufbewahren — nur damit lässt
                // sich der Agent beim nächsten Start wieder herstellen (QTMUX-85).
                // Muss VOR dem clear() geschehen, das sie sonst verwirft.
                m_agentCommand = m_inputLine.trimmed();
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

void Session::setAgentSessionRef(const QString &ref) {
    m_agentSessionRef = ref.trimmed();
}

void Session::setRestoredAgent(const QString &agentId, const QString &commandLine) {
    if (commandLine.trimmed().isEmpty()) return;
    m_agentCommand = commandLine.trimmed();

    // Kennung und Titel sofort setzen: das Login-Script schreibt später direkt ans
    // Backend und läuft damit an observeInput vorbei — ohne diese Zeilen bliebe die
    // Sidebar bei "zsh" stehen, obwohl ein Agent läuft.
    QString id = agentId;
    const AgentInfo *info = AgentRegistry::detect(m_agentCommand);
    if (id.isEmpty() && info) id = info->id;
    if (!id.isEmpty() && m_agentId != id) {
        m_agentId = id;
        emit agentChanged();
    }
    if (info) {
        m_titleFromAgent = true;
        setTitle(info->displayName);
    }
}

void Session::setLoginScript(const QString &s) {
    m_loginScript = s;
    m_loginScriptPending = !s.trimmed().isEmpty();
}

void Session::setSshPassword(const QString &pw) {
    m_sshPassword = pw;
    m_sshPasswordPending = !pw.isEmpty();
}

void Session::scanForPasswordPrompt(const QByteArray &data) {
    if (!m_sshPasswordPending) return;
    // Gleitenden Puffer der letzten Ausgabe halten (der Prompt kann über mehrere
    // Reads zerteilt ankommen) und auf das ssh-Passwort-Muster prüfen.
    m_promptScan.append(data);
    if (m_promptScan.size() > 512) m_promptScan = m_promptScan.right(512);
    const QByteArray lower = m_promptScan.toLower();
    // Deckt "<user>@<host>'s password:" und schlichtes "Password:" ab.
    if (!lower.contains("password:")) return;

    // Genau einmal senden (kein erneutes Senden bei Fehlversuch → kein Lockout).
    m_sshPasswordPending = false;
    const QByteArray pw = m_sshPassword.toUtf8();
    m_sshPassword.clear();
    m_promptScan.clear();
    if (m_backend) m_backend->write(pw + '\r');
}

void Session::armLoginScript() {
    // Einmalig nach dem ersten Output einen Fallback-Timer starten: feuert das
    // Login-Script auch ohne Shell-Integration. Kommt vorher ein OSC-133-Prompt,
    // läuft das Script schon dort (runLoginScript leert m_loginScriptPending).
    if (!m_loginScriptPending || m_loginArmed) return;
    m_loginArmed = true;
    QTimer::singleShot(800, this, [this]() { runLoginScript(); });
}

void Session::runLoginScript() {
    if (!m_loginScriptPending) return;
    m_loginScriptPending = false;   // genau einmal senden
    // Jede nicht-leere Zeile als Befehl + Enter (CR) senden. Direkt ans Backend,
    // nicht über write() — automatisierte Eingabe braucht keine Tipp-Beobachtung.
    QByteArray out;
    const QStringList lines =
        m_loginScript.split(QRegularExpression(QStringLiteral("\r\n|\r|\n")));
    for (const QString &ln : lines) {
        if (ln.trimmed().isEmpty()) continue;
        out += ln.toUtf8();
        out += '\r';
    }
    if (!out.isEmpty() && m_backend) m_backend->write(out);
}

void Session::resize(int cols, int rows) {
    if (cols == m_cols && rows == m_rows) return;
    m_cols = cols;
    m_rows = rows;
    if (m_screen) m_screen->setSize(rows, cols);
    if (m_backend) m_backend->resize(cols, rows);
    // QTMUX-120: Erst hier — und nur hier — steht die Größe fest, die auch im PTY
    // ankommt. Der Aufruf kommt aus TerminalItem::applyPendingResize, also NACH der
    // 60-ms-Entprellung aus QTMUX-86; die transienten Zwischengrößen des Layouts
    // erreichen diese Stelle nie und können die Anzeige darum nicht flackern lassen.
    emit sizeChanged();
}

} // namespace qtmux

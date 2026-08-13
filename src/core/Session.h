#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QElapsedTimer>

#include "PromptQueue.h"
#include <memory>

#include "ITerminalBackend.h"

QT_FORWARD_DECLARE_CLASS(QTimer)

namespace qtmux {

class VtScreen;

/// Vorrangregel fürs Arbeitsverzeichnis (QTMUX-108), Gui-frei und damit prüfbar:
/// Ein von der Shell SELBST gemeldeter **lokaler** Pfad (OSC 7) schlägt den gepollten
/// Wert aus `Pty::currentWorkingDirectory()`. Fehlt die Meldung — oder gehört sie zu
/// einem fremden Rechner —, bleibt es beim bisherigen Weg.
/// 🔑 Warum die Meldung gewinnt: Der gepollte Wert ist bei PowerShell schlicht falsch
/// (`Set-Location` lässt das Win32-Arbeitsverzeichnis stehen), und bei `ssh`/Containern
/// beschreibt er den lokalen Client statt dessen, was der Anwender sieht.
/// 🔑 Warum ein FREMDER Host hier nicht gewinnt: Dieser Wert wird weiterverwendet, um
/// Sessions zu persistieren und das Verzeichnis an neue Shells zu vererben — ein Pfad
/// von einer anderen Maschine existiert hier womöglich gar nicht.
QString effectiveWorkingDirectory(const QString &reported, bool reportedIsLocal,
                                  const QString &polled);

/// Eine Terminal-Session: bündelt ein Backend (PTY/SSH/Serial/App) mit seinem
/// VT-Screen und verdrahtet beide. Backend-agnostisch — die UI spricht nur mit Session.
class Session : public QObject {
    Q_OBJECT
    // Laufzeit-eindeutige Session-ID (Counter, konstant über die Session-Lebensdauer).
    // Für QML lesbar (z. B. Agenten-Benachrichtigungs-Abos im Einstellungen-Dialog).
    Q_PROPERTY(int sessionId READ id CONSTANT)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(int state READ stateInt NOTIFY stateChanged)
    Q_PROPERTY(QString agentId READ agentId NOTIFY agentChanged)
    Q_PROPERTY(bool needsAttention READ needsAttention NOTIFY attentionChanged)
    Q_PROPERTY(int activity READ activityInt NOTIFY activityChanged)
    // Backend-Typ als int (Shell 0, Ssh 1, Serial 2, App/Plugin 3) — die eingeklappte
    // Seitenleiste wählt daraus ihr Icon. NOTIFY statt CONSTANT, weil `attachBackend`
    // den Typ erst nach dem Konstruktor setzt.
    Q_PROPERTY(int backendType READ typeInt NOTIFY backendTypeChanged)
    // Zeitpunkt des letzten Aktivitätswechsels (ms seit Epoche), damit die Oberfläche
    // „wartet seit 40 s" ausrechnen kann. Wechselt genau mit `activityChanged`.
    Q_PROPERTY(qint64 activitySinceMs READ activitySinceMs NOTIFY activityChanged)
    Q_PROPERTY(QString lastNotification READ lastNotification NOTIFY notificationChanged)
    // Fortschritt (OSC 9;4): aktiv? + state (1=normal,2=Fehler,3=unbestimmt,4=pausiert) + 0..100.
    Q_PROPERTY(bool progressActive READ progressActive NOTIFY progressChanged)
    Q_PROPERTY(int progressState READ progressState NOTIFY progressChanged)
    Q_PROPERTY(int progressValue READ progressValue NOTIFY progressChanged)
    // True, wenn in dieser Session ein Agent läuft, der sich per MCP als Controller
    // angemeldet hat (Sidebar zeigt dann einen roten Tab). Reiner Laufzeitzustand.
    Q_PROPERTY(bool mcpController READ mcpController NOTIFY mcpControllerChanged)
    // Gecachtes Arbeitsverzeichnis (nur Shell-Sessions). Wird periodisch über
    // refreshWorkingDirectory() aktualisiert; Sidebar zeigt es klein unter dem Titel.
    Q_PROPERTY(QString workingDirectory READ workingDirectory NOTIFY workingDirectoryChanged)
    // Frei wählbarer Gruppenname (QTMUX-42): fasst in der Sidebar die Sessions
    // zusammen, die gemeinsam an einer Sache arbeiten. Leer = ohne Gruppe.
    Q_PROPERTY(QString group READ group NOTIFY groupChanged)
    // Window-Zugehörigkeit (QTMUX-83, B1): jede Session ist ein Pane genau eines Windows.
    // -1 = (noch) keinem Window zugeordnet. Adressierung bleibt über die stabile id().
    Q_PROPERTY(int windowId READ windowId WRITE setWindowId NOTIFY windowIdChanged)
    // Rastergröße (QTMUX-120), für das Statusleisten-Feld „80×24".
    // 🔑 Bewusst HIER und nicht am TerminalItem: Das Item meldet seine Größe erst nach
    // der 60-ms-Entprellung an die Session (QTMUX-86, applyPendingResize). Wer die
    // Anzeige an das Item bindet, sieht die transienten Zwischenwerte — beim
    // Window-Wechsel und beim Teilen also kurz Unsinn wie 80×2. Was hier steht, ist
    // genau das, was auch im PTY angekommen ist.
    Q_PROPERTY(int cols READ cols NOTIFY sizeChanged)
    Q_PROPERTY(int rows READ rows NOTIFY sizeChanged)
    // Git-Branch des Arbeitsverzeichnisses (QTMUX-58). Leer = kein Repository, kein
    // lokales Verzeichnis oder ein gemeldetes Verzeichnis auf einem FREMDEN Rechner.
    Q_PROPERTY(QString gitBranch READ gitBranch NOTIFY gitBranchChanged)
    Q_PROPERTY(bool gitDetached READ gitDetached NOTIFY gitBranchChanged)
    // Anzahl eingereihter Prompts (QTMUX-90) — treibt das Zaehler-Abzeichen auf der Kachel.
    Q_PROPERTY(int queuedCount READ queuedCount NOTIFY queueChanged)
public:
    enum class Type { Shell, Ssh, Serial, App };
    Q_ENUM(Type)

    // Werte bewusst kompatibel mit der Sidebar-Ring-Logik (1=grün, 2=gelb, 3=rot, 4=grau).
    enum class Activity { Idle = 0, Running = 1, Waiting = 2, Error = 3, Closed = 4 };
    Q_ENUM(Activity)

    explicit Session(QObject *parent = nullptr);
    ~Session() override;

    /// Übernimmt Besitz des Backends und verbindet es mit einem frischen VtScreen.
    void attachBackend(ITerminalBackend *backend, Type type, int cols, int rows);

    int id() const { return m_id; }
    Type type() const { return m_type; }
    QString title() const { return m_title; }
    /// Sichtbarer Bildschirm als Klartext (für externe Steuerung / MCP).
    QString screenText() const;
    /// Gesamte Scrollback-Historie als Klartext (für externe Steuerung / MCP).
    QString scrollbackText() const;
    /// Aktuelles Arbeitsverzeichnis (für Persistenz und die Vererbung an neue Shells):
    /// bevorzugt das von der Shell gemeldete lokale Verzeichnis (OSC 7, QTMUX-108),
    /// sonst die Abfrage am Backend. Regel in effectiveWorkingDirectory().
    QString currentWorkingDirectory() const;
    /// Gecachtes Arbeitsverzeichnis (siehe refreshWorkingDirectory). Leer bei
    /// Nicht-Shell-Sessions (SSH/Seriell/Plugin haben kein sinnvolles lokales CWD).
    QString workingDirectory() const { return m_workingDir; }
    /// Rechnername aus der letzten OSC-7-Meldung, wenn das Verzeichnis auf einem
    /// FREMDEN Rechner liegt (SSH, Container) — sonst leer. Rein informativ; der Pfad
    /// steht dann trotzdem in workingDirectory(), taugt aber nicht als lokales Ziel.
    QString workingDirectoryHost() const { return m_reportedLocal ? QString() : m_reportedHost; }
    /// Liegt das angezeigte Arbeitsverzeichnis auf einem fremden Rechner?
    bool workingDirectoryIsRemote() const { return m_cwdReported && !m_reportedLocal; }
    /// Fragt das Arbeitsverzeichnis live ab (nur Shell) und meldet bei Änderung
    /// workingDirectoryChanged. Vom SessionModel periodisch aufgerufen (CWD ändert
    /// sich nur gelegentlich → günstiges Polling statt teurer Abfrage je Repaint).
    /// Meldet die Shell selbst (OSC 7, QTMUX-108), unterbleibt das Pollen ganz — die
    /// Meldung ist genauer und würde sonst vom gepollten Wert wieder überschrieben.
    void refreshWorkingDirectory();

    /// Git-Branch des Arbeitsverzeichnisses (QTMUX-58); bei detached HEAD der kurze SHA.
    /// Leer, wenn dort kein Repository liegt.
    QString gitBranch() const { return m_gitBranch; }
    bool gitDetached() const { return m_gitDetached; }
    /// Warteschlange der Eingaben (QTMUX-90). Gehoert der Session; die Abgabe-Regel
    /// liegt Gui-frei in mayDispatchNext(), die Uhr fuer msSinceLastOutput fuehrt die
    /// Session (sie sieht als Einzige den Backend-Output).
    PromptQueue *promptQueue() const { return m_queue.get(); }
    int queuedCount() const;
    /// Reiht Text ein (oder schickt ihn sofort, wenn die Session gerade frei ist).
    /// Liefert false, wenn der Text abgewiesen wurde (leer nach dem Trimmen).
    Q_INVOKABLE bool queueText(const QString &text);
    /// Millisekunden seit der letzten Backend-Ausgabe; < 0 = noch nie etwas empfangen.
    qint64 msSinceLastOutput() const;

    /// Führt den Branch nach; vom SessionModel im selben Takt wie das CWD gerufen.
    ///
    /// 🔑 Bewusst eine EIGENE Methode und nicht Teil von refreshWorkingDirectory():
    /// Letzteres steigt bei OSC-7-Sessions sofort wieder aus (die Shell meldet ja selbst) —
    /// der Branch würde dann genau dort nie aktualisiert. Und er muss ohnehin unabhängig
    /// vom Verzeichnis geprüft werden: `git checkout` wechselt den Branch, OHNE dass sich
    /// das Arbeitsverzeichnis ändert.
    ///
    /// ⚠️ Bei einem Verzeichnis auf einem FREMDEN Rechner (OSC 7 aus SSH/Container) bleibt
    /// der Branch leer. Der gemeldete Pfad existiert hier womöglich auch — dann wäre es ein
    /// ANDERES Repository, und die Kachel zeigte einen Branch, der mit der Sitzung nichts
    /// zu tun hat. Leer ist die einzig ehrliche Antwort.
    void refreshGitBranch();

    /// PID des zugrundeliegenden Prozesses (Shell), oder -1 — für MCP-Zuordnung.
    qint64 processId() const { return m_backend ? m_backend->processId() : -1; }
    QString agentId() const { return m_agentId; }
    /// Die Kommandozeile, mit der zuletzt ein Agent gestartet wurde (z. B.
    /// "claude --dangerously-skip-permissions"). Wird beim Erkennen in observeInput
    /// gesichert und über den Blatt-`cfg` persistiert (QTMUX-85). Leer = kein Agent.
    QString agentCommand() const { return m_agentCommand; }

    /// Vom Agenten SELBST gemeldete Kennung seiner laufenden Unterhaltung
    /// (QTMUX-98, MCP `set_agent_session`). Undurchsichtige Zeichenkette — QTmux
    /// legt sie nur ab und setzt sie später in die Fortsetzungs-Vorlage ein.
    /// 🔑 QTmux kann sie NICHT selbst ermitteln: Der Agent ist ein fremdes Programm
    /// im PTY, er setzt seine ID erst zur Laufzeit (bei Claude Code sichtbar nur in
    /// der Umgebung seiner Kindprozesse), hält seine Verlaufsdatei nicht offen, und
    /// `/resume` wechselt sie mitten im Betrieb. Melden ist der einzige belastbare
    /// Weg — dieselbe Linie wie bei den Agenten-Ereignissen (QTMUX-30).
    QString agentSessionRef() const { return m_agentSessionRef; }
    void setAgentSessionRef(const QString &ref);

    /// Vermerkt einen beim Neustart wiederhergestellten Agenten (QTMUX-85): setzt
    /// Kennung, Sidebar-Titel und die gemerkte Kommandozeile.
    /// 🔑 Die Kennung MUSS explizit gesetzt werden: gestartet wird der Agent über das
    /// Login-Script, und runLoginScript() schreibt direkt ans Backend — es läuft damit
    /// an observeInput vorbei, der Agent würde sonst nie erkannt (kein Ring, kein Titel).
    /// `commandLine` ist die Original-Zeile OHNE Fortsetzungs-Argumente, damit sich
    /// `--continue` über mehrere Neustarts nicht aufsummiert; die tatsächlich
    /// abgesetzte Zeile kommt als Login-Script über setLoginScript() herein.
    void setRestoredAgent(const QString &agentId, const QString &commandLine);
    bool needsAttention() const { return m_needsAttention; }
    Activity activity() const { return m_activity; }
    int activityInt() const { return static_cast<int>(m_activity); }
    int typeInt() const { return static_cast<int>(m_type); }
    qint64 activitySinceMs() const { return m_activitySinceMs; }
    /// Hat diese Session ihren Zustand jemals SELBST gemeldet — über OSC-133-Marker der
    /// Shell-Integration oder MCP `set_activity`?
    ///
    /// ⚠️ Wichtig für alles, was aus der Aktivität Konsequenzen zieht (QTMUX-89): Der
    /// Startwert von `m_activity` ist `Running`, damit der Sidebar-Ring sofort grün ist.
    /// Ohne Shell-Integration bleibt er das für immer — eine Session am untätigen Prompt
    /// sähe also dauerhaft „beschäftigt" aus. Wer daraus etwas ableitet, muss deshalb
    /// zuerst hier fragen: ungemeldet heißt **unbekannt**, nicht „arbeitet".
    bool activityReported() const { return m_activityReported; }
    QString lastNotification() const { return m_lastNotification; }
    bool mcpController() const { return m_mcpController; }
    bool progressActive() const { return m_progressActive; }
    int progressState() const { return m_progressState; }
    int progressValue() const { return m_progressValue; }

    /// Meldet, dass in dieser Session der steuernde MCP-Agent läuft (roter Tab).
    void setMcpController(bool on);

    /// Gruppenname (QTMUX-42), leer = ohne Gruppe. Gesetzt wird er über
    /// SessionModel::setSessionGroup — nur das Model hält die Sidebar-Reihenfolge
    /// blockweise sortiert und persistiert die Zuordnung.
    QString group() const { return m_group; }
    void setGroup(const QString &g);
    int windowId() const { return m_windowId; }
    void setWindowId(int w) { if (w == m_windowId) return; m_windowId = w; emit windowIdChanged(); }

    /// Rastergröße (QTMUX-120). Bis zum ersten resize() stehen hier die Startwerte
    /// 80×24 — das ist eine Annahme, kein gemessener Wert. Sie wird binnen ~60 ms vom
    /// echten Raster des Panes abgelöst (TerminalItem::applyPendingResize).
    int cols() const { return m_cols; }
    int rows() const { return m_rows; }

    /// Beendet den zugrundeliegenden Prozess/die Verbindung (für sauberes App-Quit).
    void shutdown();

    /// Markiert die Session als aktiv (sichtbar/fokussiert). Aktivieren löscht
    /// einen anstehenden Aufmerksamkeits-Hinweis.
    void setActive(bool active);

    /// Login-Script (Auto-Befehle nach Verbindungsaufbau, QTMUX-23): eine Zeile = ein
    /// Befehl. Wird EINMAL gesendet, sobald die Shell bereit ist — bei Shell-Integration
    /// am ersten OSC-133-Prompt, sonst per Fallback-Timer nach dem ersten Output.
    /// Vor start() setzen. Gedacht für key-/agent-authentifizierte bzw. nicht-interaktive
    /// Verbindungen (bei einer Passwortabfrage könnte der Fallback-Timer zu früh senden).
    void setLoginScript(const QString &s);

    /// SSH-Passwort-Auto-Fill (QTMUX-22-Integration): das Geheimnis wird EINMAL an die
    /// erste erkannte Passwort-Eingabeaufforderung (`password:`) im Output gesendet —
    /// genau wie ein getipptes Passwort (ssh liest es von seinem PTY, mit Echo aus).
    /// Vor start() setzen; NICHT persistiert (restaurierte Sessions füllen nicht erneut
    /// aus). Nur einmal senden vermeidet Lockout-Schleifen bei falschem Passwort.
    void setSshPassword(const QString &pw);

    /// Setzt den Anzeigetitel (z. B. Portname für serielle Sessions).
    void setTitle(const QString &t);
    BackendState state() const { return m_backend ? m_backend->state() : BackendState::Closed; }
    int stateInt() const { return static_cast<int>(state()); }

    /// Vom Agenten gemeldetes Ereignis (MCP post_event ODER OSC 777;qtmux-event): in den
    /// Ereignis-Bus einspeisen, als Sidebar-Notiz spiegeln und bei 'question'/'error'
    /// Aufmerksamkeit wecken (nur wenn die Session nicht fokussiert ist). Ein Agent PUSHT
    /// so — QTmux leitet nichts aus dem Bildschirm ab (QTMUX-30).
    void reportAgentEvent(const QString &kind, const QString &text);
    /// Explizite Aufmerksamkeits-Anforderung eines Agenten (MCP needs_attention);
    /// optionaler Hinweistext landet als Sidebar-Notiz.
    void flagAttention(const QString &note = QString());
    /// Aufmerksamkeits-Markierung wieder löschen (MCP clear_attention).
    void clearAttention();
    /// Vom Agenten gepushter Dauerzustand (MCP set_activity) → färbt den Sidebar-Ring:
    /// "idle" (dim), "busy" (grün), "waiting" (amber), "error" (rot). Unbekannt = ignoriert.
    /// Nötig, weil Agenten-TUIs keine OSC-133-Marker senden (QTMUX-30, kein Scraping).
    void requestActivity(const QString &state);

    VtScreen *screen() const { return m_screen.get(); }

    /// Wiederhergestellten Scrollback übernehmen (QTMUX-130) — er wird NICHT sofort
    /// eingespielt, sondern bis zur ersten echten Terminalgröße zurückgehalten.
    ///
    /// 🔑 Warum die Verzögerung: `serializeAnsi` schreibt weiche Umbrüche bewusst OHNE
    /// CRLF, damit das Terminal beim Einspielen selbst auf die aktuelle Breite umbricht.
    /// Beim Restore steht die Session aber noch auf den Startwerten 80×24 — die echte
    /// Breite kommt erst über `TerminalItem::applyPendingResize` (Layout + 60 ms
    /// Entprellung, QTMUX-86). Sofort eingespielt würde der Verlauf also bei 80 Spalten
    /// hart in Zellen umbrochen, und `vterm_set_size` reflowt den Scrollback nicht mehr:
    /// der falsche Umbruch wäre eingefroren — und würde beim nächsten Beenden erneut
    /// gespeichert, heilte also nie von selbst.
    ///
    /// Solange der Verlauf aussteht, puffert die Session auch die Backend-Ausgabe:
    /// sonst stünde ein frisch eintreffendes Prompt ÜBER der wiederhergestellten
    /// Historie statt darunter.
    ///
    /// `lastKnownCols` ist die Breite, bei der der Dump entstand (0 = unbekannt, etwa
    /// bei einer Datei aus einer älteren Fassung). Sie greift NUR im Sicherheitsnetz —
    /// wenn nie eine Messung kommt, weil das Fenster seit dem Neustart nicht sichtbar
    /// war. Dann ist die letzte bekannte Breite die beste verfügbare Näherung; der
    /// geratene Startwert 80 wäre genau der Fehler, um den es hier geht.
    void setPendingHistory(const QByteArray &dump, int lastKnownCols = 0);
    /// true, solange ein Verlauf auf die erste echte Größe wartet. ⚠️ Wer in diesem
    /// Zustand speichert, würde einen leeren Bildschirm sichern und den Dump auf der
    /// Platte damit vernichten — siehe `SessionModel::saveHistoryFor`.
    bool hasPendingHistory() const { return m_historyPending; }

    void start(int cols, int rows);
    void write(const QByteArray &data);

    /// Schreibt `data` und schickt das abschließende Enter zeitlich abgesetzt
    /// hinterher (QTMUX-31) — sonst schlucken TUI-Anwendungen es als Teil eines
    /// vermeintlichen Einfügevorgangs. Bei leerem `data` oder `enterDelayMs <= 0`
    /// geht das Enter sofort raus. Ein noch ausstehendes Enter wird vorher
    /// nachgeholt, damit die Reihenfolge stimmt.
    void writeWithEnter(const QByteArray &data, int enterDelayMs);

    /// Schreibt `data` als EINFÜGUNG: in Bracketed-Paste-Markern (ESC[200~ … 201~),
    /// sofern die Zielanwendung DECSET 2004 aktiviert hat — libvterm sendet die
    /// Marker nur dann (derselbe Weg wie der GUI-Paste). Innerhalb des Rahmens sind
    /// eingebettete \n für die TUI inertes Paste-Material; ihre zeitbasierte
    /// Einfüge-Heuristik ist damit aus dem Spiel (Bugreport: lange send_text-
    /// Nutzlasten wurden gestückelt und Teile mitten im Strom abgeschickt).
    /// ⚠️ Ein in der Nutzlast enthaltenes ESC[201~ wird ENTFERNT — es würde den
    /// Rahmen von innen schließen, und der Rest liefe wieder als Tastendrücke
    /// (Rahmen-Ausbruch). Test: tst_pastewrite::breakoutSequenceIsStripped.
    void writePasted(const QByteArray &data);
    /// writePasted + zeitlich abgesetztes Enter (QTMUX-31). Für Text-Nutzlasten
    /// (send_text, Warteschlange); send_keys bleibt bewusst auf writeWithEnter —
    /// Tastensequenzen in Paste-Markern wären Inhalt statt Tastendrücke.
    void writePastedWithEnter(const QByteArray &data, int enterDelayMs);

    /// Standardverzögerung für das abgesetzte Enter (ms).
    static constexpr int kDefaultEnterDelayMs = 60;
    /// Größenabhängiger Enter-Default für Text-Nutzlasten: Netz für Ziele OHNE
    /// Bracketed Paste (altes conhost schluckt DECSET 2004 teils, bevor es uns
    /// erreicht) — dort muss die Frist die gestückelte Zustellung überdauern.
    /// 60 ms Basis + 1 ms je 8 Byte, gedeckelt bei 2000 ms. Nur als Default;
    /// ein explizit übergebener Wert gewinnt unverändert.
    static int pasteEnterDelayMs(qint64 bytes) {
        return static_cast<int>(qMin<qint64>(kDefaultEnterDelayMs + bytes / 8, 2000));
    }
    void resize(int cols, int rows);

public slots:
    /// OSC 7: von der Shell gemeldetes Arbeitsverzeichnis übernehmen (QTMUX-108).
    /// Öffentlich, weil es der reale Eingang für ein gemeldetes Verzeichnis ist — der
    /// VtScreen verbindet sich darauf, und Tests speisen darüber echte Meldungen ein,
    /// statt eine Test-only-Setter-Attrappe zu brauchen.
    void onWorkingDirectoryReported(const QString &path, const QString &host, bool local);

signals:
    void titleChanged(const QString &title);
    void stateChanged();
    void agentChanged();
    void attentionChanged();
    void activityChanged();
    void backendTypeChanged();
    void notificationChanged();
    void mcpControllerChanged();
    void progressChanged();
    void workingDirectoryChanged();
    void gitBranchChanged();
    void queueChanged();
    void groupChanged();
    void windowIdChanged();
    void sizeChanged();
    void bell();

private:
    void setActivity(Activity a);
    void flushPendingEnter();                   // ausstehendes Enter jetzt senden (QTMUX-31)
    void writeWithEnterImpl(const QByteArray &data, int enterDelayMs, bool asPaste);
    void raiseAttention();                      // setzt needsAttention (wenn inaktiv)
    void observeInput(const QByteArray &data);  // erkennt getippte Agenten-Kommandos
    void armLoginScript();                      // Fallback-Timer beim ersten Output starten
    void runLoginScript();                      // Login-Script einmalig senden
    void scanForPasswordPrompt(const QByteArray &data);  // SSH-Passwort-Auto-Fill
    void onBell();                              // Bell -> Aufmerksamkeit, wenn inaktiv
    void onNotify(const QString &text);         // OSC 9/777
    void onProgress(int state, int value);      // OSC 9;4
    void onPromptMarker(char kind, int exitCode); // OSC 133
    void onAgentEvent(const QString &kind, const QString &text); // OSC 777;qtmux-event
    void tryDispatchQueued();                   // QTMUX-90: Abgabe pruefen
    void onBackendData(const QByteArray &data); // QTMUX-130: puffern oder durchreichen
    void flushPendingHistory();                 // QTMUX-130: Verlauf + Puffer einspielen

    std::unique_ptr<ITerminalBackend> m_backend;
    std::unique_ptr<VtScreen> m_screen;
    Type m_type = Type::Shell;
    QString m_title = QStringLiteral("Shell");
    QString m_workingDir;      // gecachtes Arbeitsverzeichnis (Polling oder OSC 7)
    std::unique_ptr<PromptQueue> m_queue;      // QTMUX-90
    QElapsedTimer m_lastOutput;               // Uhr fuer msSinceLastOutput
    QTimer *m_dispatchTimer = nullptr;        // laeuft nur, solange etwas ansteht
    // QTMUX-130: wiederhergestellter Verlauf, der auf die erste echte Groesse wartet,
    // und die derweil zurueckgehaltene Backend-Ausgabe (Reihenfolge bleibt erhalten).
    QByteArray m_pendingHistory;
    QByteArray m_pendingOutput;
    int     m_pendingCols = 0;                // Breite, bei der der Dump entstand (0 = unbekannt)
    bool    m_historyPending = false;
    QTimer *m_historyTimer = nullptr;         // Sicherheitsnetz, falls nie ein resize kommt
    // Sicherheitsnetz-Frist. Ein Pane wird binnen ~60 ms vermessen (QTMUX-86); so lange
    // zu warten kostet nichts, deckt aber auch einen zaehen Layout-Aufbau ab.
    static constexpr int kHistoryHoldMs = 1500;
    // Notbremse: Ein Backend, das sofort losplappert, darf den Puffer nicht unbegrenzt
    // wachsen lassen. Wird sie erreicht, gilt die Reihenfolge als verloren — das ist
    // immer noch besser als unbegrenzter Speicher.
    static constexpr int kMaxPendingOutput = 1 << 20;   // 1 MiB
    QString m_gitBranch;       // QTMUX-58: Branch bzw. kurzer SHA (detached)
    bool    m_gitDetached = false;
    // OSC 7 (QTMUX-108): hat die Shell ihr Verzeichnis je selbst gemeldet, und gehört
    // die Meldung zu dieser Maschine? Ungemeldet heißt „unbekannt" — dann bleibt der
    // bisherige Polling-Weg unverändert in Kraft.
    bool m_cwdReported = false;
    bool m_reportedLocal = true;
    QString m_reportedDir;     // zuletzt gemeldeter Pfad (getrennt vom Anzeige-Cache)
    QString m_reportedHost;
    QString m_agentId;
    QString m_agentCommand;    // zuletzt erkannte Agenten-Kommandozeile (QTMUX-85)
    QString m_agentSessionRef; // vom Agenten gemeldete Unterhaltungs-ID (QTMUX-98)
    QString m_group;           // Sidebar-Gruppe (QTMUX-42), leer = ohne Gruppe
    int m_windowId = -1;       // Window-Zugehörigkeit (QTMUX-83, B1), -1 = keins
    QString m_inputLine;       // Puffer der aktuell getippten Zeile
    QString m_loginScript;     // Auto-Befehle nach Verbindungsaufbau (QTMUX-23)
    bool m_loginScriptPending = false;  // Login-Script noch zu senden?
    bool m_loginArmed = false;          // Fallback-Timer bereits gestartet?
    QString m_sshPassword;              // SSH-Passwort-Auto-Fill (nur entsperrt, flüchtig)
    bool m_sshPasswordPending = false;  // Passwort noch zu senden?
    QByteArray m_promptScan;            // gleitender Output-Puffer für die Prompt-Erkennung
    bool m_titleFromAgent = false;
    bool m_active = false;
    bool m_needsAttention = false;
    bool m_mcpController = false;
    bool m_progressActive = false;
    int m_progressState = 0;     // 1=normal,2=Fehler,3=unbestimmt,4=pausiert
    int m_progressValue = 0;     // 0..100
    Activity m_activity = Activity::Running;
    // Seit wann gilt m_activity? Beim Anlegen „jetzt", danach bei jedem Wechsel gesetzt.
    qint64 m_activitySinceMs = QDateTime::currentMSecsSinceEpoch();
    bool m_activityReported = false;   // s. activityReported()
    /// Merkt „ab jetzt meldet diese Session selbst" und stößt dabei EINMAL
    /// activityChanged an. Nötig, weil die erste Meldung oft `busy` ist und damit den
    /// Startwert `Running` gar nicht ändert: setActivity meldete dann nichts, und alles,
    /// was an activityChanged hängt (Ruhezustands-Sperre, QTMUX-89), liefe nie an.
    void markActivityReported();
    QString m_lastNotification;
    bool m_commandRunning = false;   // zwischen OSC 133;C und ;D
    bool m_enterPending = false;     // abgesetztes Enter noch offen (QTMUX-31)
    int m_cols = 80;
    int m_rows = 24;
    int m_id = nextId();
    static int nextId();
};

} // namespace qtmux

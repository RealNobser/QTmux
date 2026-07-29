#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QVariantList>
#include <QVariantMap>
#include <qqmlintegration.h>

#include "SleepInhibitor.h"   // Mitglied, daher vollständige Definition nötig

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

namespace qtmux {

class Session;

/// Liste aller Sessions — speist die vertikale Sidebar und liefert Session-Objekte
/// zum Binden an TerminalItem.session.
class SessionModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool preventSleep READ preventSleep WRITE setPreventSleep NOTIFY preventSleepChanged)
    Q_PROPERTY(bool sleepInhibited READ sleepInhibited NOTIFY sleepInhibitedChanged)
public:
    enum Roles {
        TitleRole = Qt::UserRole + 1,
        IdRole,
        StateRole,
        TypeRole,
        AgentRole,
        AttentionRole,
        NotificationRole,
        McpControllerRole,
        ProgressActiveRole,
        ProgressStateRole,
        ProgressValueRole,
        WorkingDirRole,
        GroupRole,
        SessionRole,
    };
    Q_ENUM(Roles)

    explicit SessionModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(m_sessions.size()); }

    /// Erzeugt eine lokale Shell-Session und gibt die Zeile zurück.
    /// `workingDir` leer = Home; `program` leer = plattformübliche Standard-Shell.
    /// `loginScript` (optional): Auto-Befehle nach dem Start (QTMUX-23).
    Q_INVOKABLE int createShellSession(const QString &workingDir = {},
                                       const QString &program = {},
                                       const QString &loginScript = {});
    /// Auf dieser Plattform auswählbare Shells als Liste von {program, name}-Maps
    /// (für die Shell-Auswahl in der UI). Siehe qtmux::ShellRegistry.
    Q_INVOKABLE QVariantList availableShells() const;
    /// Erzeugt eine serielle Session (Port + Baudrate) und gibt deren Zeilenindex zurück.
    Q_INVOKABLE int createSerialSession(const QString &portName, int baud,
                                        const QString &loginScript = {});
    /// Erzeugt eine SSH-Session (System-ssh) und gibt deren Zeilenindex zurück.
    Q_INVOKABLE int createSshSession(const QString &host, int port,
                                     const QString &user, const QString &identityFile = {},
                                     const QString &loginScript = {},
                                     const QString &password = {});
    /// Verfügbare serielle Ports (z. B. "/dev/cu.usbserial-XYZ").
    Q_INVOKABLE QStringList availableSerialPorts() const;
    /// Erzeugt eine Plugin-Session (QTMUX-8): das Backend liefert das Plugin
    /// (PluginHost::createBackend). Gibt die Zeile zurück, oder -1 wenn
    /// Plugin/Typ nicht verfügbar (z. B. Plugin nach Restore nicht installiert).
    Q_INVOKABLE int createPluginSession(const QString &pluginId, const QString &typeId);
    /// Session-Objekt einer Zeile (für Binding an TerminalItem.session).
    Q_INVOKABLE QObject *sessionAt(int row) const;
    /// Zeilenindex einer Session-ID, oder -1.
    int rowForId(int id) const;
    /// Session-Objekt zu einer stabilen ID (für externe Steuerung / MCP).
    Session *sessionById(int id) const;
    const QList<Session *> &sessions() const { return m_sessions; }
    Q_INVOKABLE void closeSession(int row);
    /// Schreibt `data` an ALLE Sessions (Broadcast-/Sync-Input). Genutzt vom
    /// Broadcast-Modus: einmal getippt → alle Sessions erhalten dieselbe Eingabe.
    Q_INVOKABLE void writeToAll(const QByteArray &data);
    /// Verschiebt die Session von `from` an die Zielposition `to` (Drag-Reorder
    /// in der Sidebar). Persistiert die neue Reihenfolge. Die Session übernimmt
    /// dabei die Gruppe ihrer neuen Nachbarschaft (QTMUX-42) — Ziehen in einen
    /// Gruppenblock ist damit zugleich das Aufnehmen in die Gruppe.
    Q_INVOKABLE void moveSession(int from, int to);

    /// Ordnet die Session einer Gruppe zu (leerer Name = ohne Gruppe, QTMUX-42)
    /// und rückt sie ans Ende ihres Gruppenblocks. Die Sidebar zeigt Gruppen über
    /// ListView-Sections, die zusammenhängende Blöcke voraussetzen — deshalb hält
    /// das Model die Blöcke, nicht die View.
    Q_INVOKABLE void setSessionGroup(int row, const QString &name);
    /// Alle vergebenen Gruppennamen in Sidebar-Reihenfolge (ohne Leereintrag).
    Q_INVOKABLE QStringList groups() const;
    /// Anzahl der Sessions in einer Gruppe (für die Kopfzeile der Sidebar).
    Q_INVOKABLE int groupSize(const QString &name) const;
    /// Benennt eine Gruppe um (alle Mitglieder). Leerer Zielname löst sie auf.
    Q_INVOKABLE void renameGroup(const QString &from, const QString &to);
    /// Verschiebt eine ganze Gruppe als Block in der Sidebar-Reihenfolge: dir < 0 = hoch,
    /// dir > 0 = runter. Die Gruppe springt jeweils über die benachbarte „Sektion"
    /// (eine andere Gruppe ODER einen zusammenhängenden Lauf gruppenloser Sessions); die
    /// Mitglieder bleiben zusammenhängend. No-op am Rand oder bei unbekannter Gruppe.
    Q_INVOKABLE void moveGroup(const QString &name, int dir);
    /// Verschiebt die Gruppe `name` als Block vor/nach die Sektion, in der `targetRow`
    /// liegt (für Header-Drag-to-Reorder). Landet der Ziel-Zeilenwert in der Gruppe
    /// selbst, passiert nichts. `moveGroup` ist ein Spezialfall hiervon.
    Q_INVOKABLE void moveGroupToRow(const QString &name, int targetRow);
    /// Entfernt die MCP-Controller-Markierung (roter Tab) einer Zeile. Nötig, weil ein
    /// steuernder Agent, der ohne Abmeldung verschwindet, den Tab sonst hängen lässt und
    /// es keinen anderen Menschen-Weg gibt, ihn zu löschen (attach_controller ist MCP-only).
    Q_INVOKABLE void clearMcpController(int row);
    /// Beendet alle laufenden Prozesse/Verbindungen (beim App-Quit aufzurufen,
    /// nach saveState()). Verhindert verwaiste Shells/Agenten.
    Q_INVOKABLE void shutdownAll();
    /// Markiert die Zeile als aktiv/fokussiert (alle anderen inaktiv) — löscht deren Attention.
    Q_INVOKABLE void setActiveRow(int row);

    /// Stellt die Sessions aus den persistierten Einstellungen wieder her.
    /// Gibt die wiederherzustellende aktive Zeile zurück (oder -1, wenn nichts gespeichert).
    Q_INVOKABLE int restoreState();
    /// Schreibt den aktuellen Zustand (Session-Liste + aktive Zeile) in die Einstellungen.
    Q_INVOKABLE void saveState() const;

    // --- Per-Window-Persistenz (QTMUX-83, Stufe 3) --------------------------
    /// Persistierbare Beschreibung der Session als Map (für das Blatt-`cfg` des
    /// Window-Baums). Das Arbeitsverzeichnis wird bei Shells LIVE abgefragt.
    Q_INVOKABLE QVariantMap sessionConfig(int row) const;
    /// Schreibt den farbgetreuen Scrollback der Session in <historyDir>/<key>.ans.
    /// `key` = paneId (stabil über Neustarts) statt des flüchtigen Save-Index.
    Q_INVOKABLE void saveHistoryFor(int row, int key) const;
    /// Liest <historyDir>/<key>.ans und speist ihn in den Screen ein (vor erster
    /// Backend-Ausgabe aufrufen, damit er als Scrollback nach oben rollt).
    Q_INVOKABLE void loadHistoryFor(int row, int key) const;

    /// „Bildschirm leeren" unter Erhalt des Scrollbacks (QTMUX-61). Wirkt direkt auf den
    /// Screen der Session — es wird **nichts** an die Shell geschickt: ein getipptes `clear`
    /// verwirft je nach Agent/TUI den Verlauf und würde außerdem in der Eingabezeile eines
    /// laufenden Agenten landen. Liefert false, wenn es nichts zu tun gab.
    Q_INVOKABLE bool clearViewport(int row) const;
    /// Entfernt alle <historyDir>/<n>.ans, deren n NICHT in `keys` (den aktuellen
    /// paneIds) liegt — räumt Dumps geschlossener Panes weg.
    Q_INVOKABLE void pruneHistoryExcept(const QList<int> &keys) const;

    // --- Agenten-Wiederherstellung (QTMUX-85) -------------------------------
    /// Die tatsächlich abzusetzende Startzeile eines Agenten: um die Argumente des
    /// gewählten Fortsetzungs-Modus ergänzt (qtmux::ResumeMode als int), sonst
    /// unverändert. `sessionRef` wird nur im Modus „gemeldete Sitzung" gebraucht.
    /// Gedacht als `loginScript`-Argument von createShellSession/createSshSession —
    /// nur dort steht das Kommando VOR dem Start fest, und nur dann greift die
    /// erprobte Auslösung am ersten Prompt zuverlässig.
    Q_INVOKABLE QString agentLaunchCommand(const QString &commandLine, int mode,
                                           const QString &sessionRef = {}) const;

    /// Vermerkt den wiederhergestellten Agenten an `row`: setzt Kennung und Titel der
    /// Session (das Login-Script läuft an der Tipp-Beobachtung vorbei) und hält
    /// Kennung, Befehl und Unterhaltungs-Referenz im SessionConfig fest, damit sie
    /// erneut persistiert werden.
    Q_INVOKABLE void markRestoredAgent(int row, const QString &agentId,
                                       const QString &commandLine,
                                       const QString &sessionRef = {});

    /// Merkt den gespeicherten Agenten NUR im SessionConfig vor — ohne ihn zu starten.
    /// Nötig, wenn die Wiederherstellung abgeschaltet ist: sonst schriebe das Beenden
    /// den leeren Laufzeitwert zurück und löschte den gespeicherten Befehl.
    Q_INVOKABLE void seedAgentConfig(int row, const QString &agentId,
                                     const QString &commandLine,
                                     const QString &sessionRef = {});

    /// Übernimmt die vom Agenten gemeldete Unterhaltungs-ID (MCP `set_agent_session`,
    /// QTMUX-98) in Session UND SessionConfig. Der Agent meldet sie bei jedem Wechsel
    /// erneut — insbesondere nach `/resume` und `/clear`, die sie zur Laufzeit ändern.
    Q_INVOKABLE void setAgentSessionRef(int row, const QString &sessionRef);

    /// Schaltet den Restore-Modus (QTMUX-85): unterdrückt die CWD-Vererbung von der
    /// aktiven Session und das redundante Zurückschreiben des Alt-Schemas, während
    /// QML die Windows wiederherstellt. Der Guard existierte bisher nur im
    /// (seit dem Window-Modell toten) restoreState()-Pfad.
    Q_INVOKABLE void setRestoring(bool on) { m_restoring = on; }

    // --- Ruhezustand verhindern (QTMUX-89) ---------------------------------------
    /// Anwender-Schalter, **Vorgabe AUS**. Ist er an, hält QTmux das System wach,
    /// solange mindestens eine Session `busy` meldet — die Regel steht Gui-frei in
    /// [SleepInhibitor.h](../core/SleepInhibitor.h) (`shouldPreventSleep`).
    bool preventSleep() const { return m_preventSleep; }
    void setPreventSleep(bool on);
    /// Hält die Sperre gerade? Für die Anzeige — so ist nachvollziehbar, warum der
    /// Rechner wach bleibt, statt dass es wie ein Fehler wirkt.
    bool sleepInhibited() const;
    /// Kann diese Plattform das überhaupt? (Linux: derzeit nein.)
    Q_INVOKABLE bool sleepInhibitSupported() const;

signals:
    void preventSleepChanged();
    /// Die Sperre wurde gesetzt oder freigegeben.
    void sleepInhibitedChanged();
    void countChanged();
    /// Eine Gruppenzuordnung hat sich geändert (QTMUX-42). groups()/groupSize()
    /// sind Funktionen, keine Properties — QML-Bindungen darauf brauchen diesen
    /// Anker, sonst zeigen Kopfzeilen und Menüs veraltete Stände.
    void groupsChanged();
    /// Eine (nicht-fokussierte) Session fordert Aufmerksamkeit — für Fenster-Alert.
    void attentionRaised(int row);

private:
    void wireSession(Session *s, int row);

    /// Verzeichnis für die farbgetreuen Scrollback-Dumps (per Profil getrennt über den
    /// App-Namen). Session-Restore Stufe 2 (QTMUX-81).
    QString historyDir() const;
    /// Beim App-Quit je Session den ANSI-Verlauf (Scrollback+Screen) in <historyDir>/<i>.ans
    /// schreiben; überzählige Dateien einer früheren, längeren Liste werden entfernt.
    void saveHistory() const;

    /// Persistierbare Beschreibung einer Session (Inhalt ist nicht wiederherstellbar).
    struct SessionConfig {
        int type = 0;        // qtmux::Session::Type
        QString serialPort;  // nur bei Seriell
        int baud = 115200;   // nur bei Seriell
        QString workingDir;  // letztes Arbeitsverzeichnis (Shell), live in saveState() erfasst
        QString program;     // gewählte Shell (Shell), leer = Standard-Shell
        QString host;        // SSH
        int sshPort = 22;    // SSH
        QString user;        // SSH
        QString identity;    // SSH (Identity-Datei, optional)
        QString pluginId;    // Plugin-Session (QTMUX-8): Plugin- und
        QString pluginType;  // Backend-Typ-ID für die Wiederherstellung
        QString group;       // Sidebar-Gruppe (QTMUX-42), leer = ohne Gruppe
        // Agent aus dem gespeicherten Zustand (QTMUX-85). Dient als RÜCKFALL für
        // sessionConfig(): Wurde die Session ohne Agenten wiederhergestellt (Schalter
        // aus), hat sie zur Laufzeit keinen — ohne diesen Halt würde das Beenden den
        // gespeicherten Befehl mit Leer überschreiben und ein späteres Einschalten
        // des Schalters liefe ins Nichts.
        QString agentId;
        QString agentCommand;
        QString agentSessionRef;  // vom Agenten gemeldete Unterhaltungs-ID (QTMUX-98)
    };

    /// Verschiebt die Zeile ans Ende des Blocks ihrer eigenen Gruppe (bzw. für
    /// gruppenlose Sessions ans Listenende) und meldet die neue Zeile zurück.
    int regroupRow(int row);
    /// Reines Verschieben ohne Gruppen-Adoption (die macht nur moveSession).
    bool moveRowInternal(int from, int to);
    /// Verschiebt den zusammenhängenden Zeilenblock [first..last] vor die Zielzeile `dest`
    /// (in aktueller Nummerierung) — als EIN beginMoveRows. Für moveGroup.
    bool moveBlock(int first, int last, int dest);

    QList<Session *> m_sessions;
    QList<SessionConfig> m_configs;   // parallel zu m_sessions
    QTimer *m_cwdPoll = nullptr;      // pollt das Arbeitsverzeichnis aller Sessions

    /// Sperre nachführen: bei jedem Aktivitätswechsel, beim Anlegen/Schließen einer
    /// Session und beim Umlegen des Schalters. Idempotent — setActive prüft selbst.
    void updateSleepInhibit();
    bool m_preventSleep = false;      // Vorgabe AUS (Anwender-Entscheidung)
    SleepInhibitor m_sleepInhibitor;
    int m_activeRow = -1;
    bool m_restoring = false;         // unterdrückt Persistierung während restoreState()
    bool m_shuttingDown = false;      // App-Quit: kein Auto-Remove/Save mehr (Zustand ist gesichert)
};

} // namespace qtmux

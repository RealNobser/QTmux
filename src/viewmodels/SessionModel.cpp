#include "SessionModel.h"
#include "Session.h"
#include "PluginHost.h"
#include "PtyBackend.h"
#include "SerialBackend.h"
#include "SshBackend.h"
#include "ShellRegistry.h"

#include <QSerialPortInfo>
#include <QSettings>
#include <QTimer>
#include <QVariantMap>

namespace qtmux {

SessionModel::SessionModel(QObject *parent) : QAbstractListModel(parent) {
    // Arbeitsverzeichnis der Shell-Sessions periodisch nachführen. CWD ändert sich
    // nur gelegentlich (nach cd), daher genügt ein träges Intervall; jede Session
    // meldet via workingDirectoryChanged nur echte Änderungen (kein Repaint-Spam).
    m_cwdPoll = new QTimer(this);
    m_cwdPoll->setInterval(1500);
    connect(m_cwdPoll, &QTimer::timeout, this, [this]() {
        for (Session *s : m_sessions) s->refreshWorkingDirectory();
    });
    m_cwdPoll->start();
}

int SessionModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : count();
}

QVariant SessionModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= count()) return {};
    Session *s = m_sessions.at(index.row());
    switch (role) {
    case TitleRole:   return s->title();
    case StateRole:   return s->activityInt();   // Sidebar-Ring folgt der Aktivität
    case TypeRole:    return static_cast<int>(s->type());
    case AgentRole:   return s->agentId();
    case AttentionRole: return s->needsAttention();
    case NotificationRole: return s->lastNotification();
    case McpControllerRole: return s->mcpController();
    case ProgressActiveRole: return s->progressActive();
    case ProgressStateRole: return s->progressState();
    case ProgressValueRole: return s->progressValue();
    case WorkingDirRole: return s->workingDirectory();
    case SessionRole: return QVariant::fromValue(static_cast<QObject *>(s));
    default:          return {};
    }
}

QHash<int, QByteArray> SessionModel::roleNames() const {
    return {
        {TitleRole,   "title"},
        {StateRole,   "runState"},
        {TypeRole,    "sessionType"},
        {AgentRole,   "agentId"},
        {AttentionRole, "needsAttention"},
        {NotificationRole, "lastNotification"},
        {McpControllerRole, "mcpController"},
        {ProgressActiveRole, "progressActive"},
        {ProgressStateRole, "progressState"},
        {ProgressValueRole, "progressValue"},
        {WorkingDirRole, "workingDir"},
        {SessionRole, "session"},
    };
}

void SessionModel::wireSession(Session *s, int row) {
    // Bei Titel-/Zustandswechsel die betroffene Sidebar-Zeile aktualisieren.
    auto refresh = [this, s]() {
        const int r = static_cast<int>(m_sessions.indexOf(s));
        if (r >= 0) {
            const QModelIndex idx = index(r);
            emit dataChanged(idx, idx,
                {TitleRole, StateRole, AgentRole, AttentionRole, NotificationRole,
                 McpControllerRole, ProgressActiveRole, ProgressStateRole, ProgressValueRole,
                 WorkingDirRole});
        }
    };
    connect(s, &Session::titleChanged, this, refresh);
    connect(s, &Session::stateChanged, this, refresh);
    connect(s, &Session::agentChanged, this, refresh);
    connect(s, &Session::activityChanged, this, refresh);
    connect(s, &Session::notificationChanged, this, refresh);
    connect(s, &Session::attentionChanged, this, refresh);
    connect(s, &Session::mcpControllerChanged, this, refresh);
    connect(s, &Session::progressChanged, this, refresh);
    connect(s, &Session::workingDirectoryChanged, this, refresh);

    // Steigt die Aufmerksamkeit, das Fenster informieren (Dock-/Taskbar-Alert).
    connect(s, &Session::attentionChanged, this, [this, s]() {
        if (s->needsAttention()) {
            const int r = static_cast<int>(m_sessions.indexOf(s));
            if (r >= 0) emit attentionRaised(r);
        }
    });

    // Endet die zugrundeliegende Shell/Verbindung, die Session automatisch entfernen.
    // Verzögert (QueuedConnection), um nicht während der Signalauslösung zu löschen.
    // NICHT beim App-Quit: dort hat saveState() den Zustand bereits gesichert; ein
    // Auto-Remove würde die gerade gespeicherte Session-Liste wieder leeren.
    connect(s, &Session::stateChanged, this, [this, s]() {
        if (m_shuttingDown) return;
        if (s->state() == BackendState::Closed) {
            QMetaObject::invokeMethod(this, [this, s]() {
                if (m_shuttingDown) return;
                const int r = static_cast<int>(m_sessions.indexOf(s));
                if (r >= 0) closeSession(r);
            }, Qt::QueuedConnection);
        }
    });
    Q_UNUSED(row);
}

int SessionModel::createShellSession(const QString &workingDir, const QString &program,
                                     const QString &loginScript) {
    auto *s = new Session(this);
    auto *pty = new PtyBackend();
    if (!workingDir.isEmpty()) pty->setWorkingDirectory(workingDir);
    if (!program.isEmpty()) pty->setProgram(program);   // leer = Standard-Shell
    // Lokale Shells als Login-Shell starten (wie Terminal.app/iTerm) → ~/.zprofile,
    // /etc/zprofile (path_helper/Homebrew-PATH), ~/.bash_profile … werden geladen.
    pty->setLoginShell(true);
    s->setLoginScript(loginScript);
    // Eigene Session-ID in die Shell-Umgebung legen: ein steuernder Agent liest
    // $QTMUX_SESSION_ID und meldet sich per MCP-Tool attach_controller(id) an.
    pty->setExtraEnv({QStringLiteral("QTMUX_SESSION_ID=%1").arg(s->id())});
    s->attachBackend(pty, Session::Type::Shell, 80, 24);
    // Initialtitel aus dem gestarteten Programm ableiten (z. B. "PowerShell",
    // "Eingabeaufforderung", "Bash") — Session::setTitle verschönert ihn. So zeigt
    // die Sidebar sofort einen Namen statt eines Pfads, auch bevor ein OSC-Titel kommt.
    s->setTitle(program.isEmpty() ? PtyBackend::defaultShell() : program);

    const int row = count();
    beginInsertRows({}, row, row);
    m_sessions.append(s);
    SessionConfig cfg;
    cfg.type = static_cast<int>(Session::Type::Shell);
    cfg.workingDir = workingDir;
    cfg.program = program;
    m_configs.append(cfg);
    endInsertRows();

    wireSession(s, row);
    s->start(80, 24);
    emit countChanged();
    saveState();
    return row;
}

QVariantList SessionModel::availableShells() const {
    QVariantList out;
    for (const ShellInfo &sh : ShellRegistry::available()) {
        QVariantMap m;
        m[QStringLiteral("program")] = sh.program;
        m[QStringLiteral("name")] = sh.name;
        out.append(m);
    }
    return out;
}

int SessionModel::createSerialSession(const QString &portName, int baud,
                                      const QString &loginScript) {
    auto *s = new Session(this);
    auto *serial = new SerialBackend();
    serial->setPortName(portName);
    serial->setBaudRate(baud);
    s->setLoginScript(loginScript);
    s->attachBackend(serial, Session::Type::Serial, 80, 24);
    s->setTitle(portName);

    const int row = count();
    beginInsertRows({}, row, row);
    m_sessions.append(s);
    m_configs.append({static_cast<int>(Session::Type::Serial), portName, baud});
    endInsertRows();

    wireSession(s, row);
    s->start(80, 24);
    emit countChanged();
    saveState();
    return row;
}

int SessionModel::createSshSession(const QString &host, int port,
                                   const QString &user, const QString &identityFile,
                                   const QString &loginScript, const QString &password) {
    auto *s = new Session(this);
    auto *ssh = new SshBackend();
    ssh->setHost(host);
    ssh->setPort(port > 0 ? port : 22);
    ssh->setUser(user);
    ssh->setIdentityFile(identityFile);
    s->setLoginScript(loginScript);
    s->setSshPassword(password);   // Vault-Auto-Fill (leer = keins; nicht persistiert)
    s->attachBackend(ssh, Session::Type::Ssh, 80, 24);
    s->setTitle(ssh->target());

    const int row = count();
    beginInsertRows({}, row, row);
    m_sessions.append(s);
    SessionConfig cfg;
    cfg.type = static_cast<int>(Session::Type::Ssh);
    cfg.host = host;
    cfg.sshPort = port > 0 ? port : 22;
    cfg.user = user;
    cfg.identity = identityFile;
    m_configs.append(cfg);
    endInsertRows();

    wireSession(s, row);
    s->start(80, 24);
    emit countChanged();
    saveState();
    return row;
}

int SessionModel::createPluginSession(const QString &pluginId, const QString &typeId) {
    // Backend kommt vom Plugin (QTMUX-8); ohne Plugin/Typ keine Session — beim
    // Restore mit deinstalliertem Plugin wird der Eintrag so still übersprungen.
    ITerminalBackend *backend = PluginHost::instance().createBackend(pluginId, typeId);
    if (!backend) return -1;

    auto *s = new Session(this);
    s->attachBackend(backend, Session::Type::App, 80, 24);
    const QString name = PluginHost::instance().backendTypeName(pluginId, typeId);
    s->setTitle(name.isEmpty() ? pluginId : name);

    const int row = count();
    beginInsertRows({}, row, row);
    m_sessions.append(s);
    SessionConfig cfg;
    cfg.type = static_cast<int>(Session::Type::App);
    cfg.pluginId = pluginId;
    cfg.pluginType = typeId;
    m_configs.append(cfg);
    endInsertRows();

    wireSession(s, row);
    s->start(80, 24);
    emit countChanged();
    saveState();
    return row;
}

QStringList SessionModel::availableSerialPorts() const {
    QStringList ports;
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) ports << info.portName();
    return ports;
}

QObject *SessionModel::sessionAt(int row) const {
    if (row < 0 || row >= count()) return nullptr;
    return m_sessions.at(row);
}

int SessionModel::rowForId(int id) const {
    for (int i = 0; i < count(); ++i)
        if (m_sessions.at(i)->id() == id) return i;
    return -1;
}

Session *SessionModel::sessionById(int id) const {
    const int r = rowForId(id);
    return r >= 0 ? m_sessions.at(r) : nullptr;
}

void SessionModel::setActiveRow(int row) {
    for (int i = 0; i < count(); ++i) {
        m_sessions.at(i)->setActive(i == row);
    }
    if (row != m_activeRow) {
        m_activeRow = row;
        saveState();
    }
}

void SessionModel::closeSession(int row) {
    if (row < 0 || row >= count()) return;
    beginRemoveRows({}, row, row);
    Session *s = m_sessions.takeAt(row);
    m_configs.removeAt(row);
    endRemoveRows();
    s->deleteLater();
    emit countChanged();
    saveState();
}

void SessionModel::moveSession(int from, int to) {
    if (from < 0 || from >= count() || to < 0 || to >= count() || from == to) return;
    // beginMoveRows erwartet bei Abwärtsbewegung den Zielindex +1.
    const int dest = to > from ? to + 1 : to;
    if (!beginMoveRows({}, from, from, {}, dest)) return;
    m_sessions.move(from, to);
    m_configs.move(from, to);
    endMoveRows();

    // Aktive Zeile mitführen.
    if (m_activeRow == from) m_activeRow = to;
    else if (from < to && m_activeRow > from && m_activeRow <= to) --m_activeRow;
    else if (to < from && m_activeRow >= to && m_activeRow < from) ++m_activeRow;

    saveState();
}

void SessionModel::writeToAll(const QByteArray &data) {
    for (Session *s : m_sessions)
        if (s) s->write(data);
}

void SessionModel::shutdownAll() {
    // Nur Prozesse beenden (keine Modelländerung) — wird beim App-Quit aufgerufen.
    // m_shuttingDown verhindert, dass die dabei ausgelösten Closed-Signale die
    // (zuvor von saveState gesicherte) Session-Liste per Auto-Remove leeren.
    m_shuttingDown = true;
    for (Session *s : m_sessions) s->shutdown();
}

// --- Persistenz -------------------------------------------------------------

void SessionModel::saveState() const {
    if (m_restoring || m_shuttingDown) return;   // weder beim Restore noch beim Quit überschreiben
    QSettings s;
    s.beginWriteArray(QStringLiteral("sessions"), m_configs.size());
    for (int i = 0; i < m_configs.size(); ++i) {
        const SessionConfig &cfg = m_configs.at(i);
        s.setArrayIndex(i);
        s.setValue(QStringLiteral("type"), cfg.type);
        s.setValue(QStringLiteral("serialPort"), cfg.serialPort);
        s.setValue(QStringLiteral("baud"), cfg.baud);
        s.setValue(QStringLiteral("host"), cfg.host);
        s.setValue(QStringLiteral("sshPort"), cfg.sshPort);
        s.setValue(QStringLiteral("user"), cfg.user);
        s.setValue(QStringLiteral("identity"), cfg.identity);
        // Aktuelles Arbeitsverzeichnis live abfragen (Shell), sonst gespeichertes verwenden.
        QString dir = cfg.workingDir;
        if (cfg.type == static_cast<int>(Session::Type::Shell) && i < m_sessions.size()) {
            const QString live = m_sessions.at(i)->currentWorkingDirectory();
            if (!live.isEmpty()) dir = live;
        }
        s.setValue(QStringLiteral("workingDir"), dir);
        s.setValue(QStringLiteral("program"), cfg.program);
        s.setValue(QStringLiteral("pluginId"), cfg.pluginId);
        s.setValue(QStringLiteral("pluginType"), cfg.pluginType);
    }
    s.endArray();
    s.setValue(QStringLiteral("sessions/activeRow"), m_activeRow);
}

int SessionModel::restoreState() {
    QSettings s;
    const int n = s.beginReadArray(QStringLiteral("sessions"));
    m_restoring = true;
    for (int i = 0; i < n; ++i) {
        s.setArrayIndex(i);
        const int type = s.value(QStringLiteral("type")).toInt();
        if (type == static_cast<int>(Session::Type::Serial)) {
            createSerialSession(s.value(QStringLiteral("serialPort")).toString(),
                                s.value(QStringLiteral("baud"), 115200).toInt());
        } else if (type == static_cast<int>(Session::Type::Ssh)) {
            createSshSession(s.value(QStringLiteral("host")).toString(),
                             s.value(QStringLiteral("sshPort"), 22).toInt(),
                             s.value(QStringLiteral("user")).toString(),
                             s.value(QStringLiteral("identity")).toString());
        } else if (type == static_cast<int>(Session::Type::App)) {
            // Plugin-Session: nur wiederherstellen, wenn das Plugin (noch) geladen
            // ist — createPluginSession gibt sonst -1 zurück und überspringt still.
            createPluginSession(s.value(QStringLiteral("pluginId")).toString(),
                                s.value(QStringLiteral("pluginType")).toString());
        } else {
            createShellSession(s.value(QStringLiteral("workingDir")).toString(),
                               s.value(QStringLiteral("program")).toString());
        }
    }
    m_restoring = false;
    s.endArray();

    m_activeRow = s.value(QStringLiteral("sessions/activeRow"), n > 0 ? 0 : -1).toInt();
    if (m_activeRow >= count()) m_activeRow = count() - 1;
    return m_activeRow;
}

} // namespace qtmux

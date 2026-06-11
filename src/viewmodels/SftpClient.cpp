#include "SftpClient.h"

#include <QFileInfo>
#include <QUrl>
#include <QTimer>

namespace qtmux {

SftpClient::SftpClient(QObject *parent) : QObject(parent) {
    connect(&m_pty, &Pty::readyRead, this, &SftpClient::onData);
    connect(&m_pty, &Pty::finished, this, &SftpClient::onFinished);
}

SftpClient::~SftpClient() { m_pty.terminate(); }

void SftpClient::setBusy(bool b) {
    if (b == m_busy) return;
    m_busy = b;
    emit busyChanged();
}

void SftpClient::setStatus(const QString &s) {
    if (s == m_status) return;
    m_status = s;
    emit statusChanged();
}

QString SftpClient::remoteJoin(const QString &base, const QString &name) {
    if (name.startsWith(QLatin1Char('/'))) return name;
    QString b = base.isEmpty() ? QStringLiteral(".") : base;
    if (!b.endsWith(QLatin1Char('/'))) b += QLatin1Char('/');
    return b + name;
}

void SftpClient::connectTo(const QString &host, int port, const QString &user,
                           const QString &identity, const QString &password) {
    if (host.isEmpty()) { emit error(tr("Kein Host angegeben.")); return; }
    if (m_pty.isRunning()) close();

    m_password = password;
    // Passwort nur „pending", wenn kein Schlüssel angegeben ist (sonst Key/Agent-Auth).
    m_passwordPending = !password.isEmpty() && identity.isEmpty();
    m_connected = false;
    m_entries.clear();
    emit entriesChanged();

    QStringList args;
    if (port > 0 && port != 22) args << QStringLiteral("-P") << QString::number(port);
    if (!identity.isEmpty()) args << QStringLiteral("-i") << identity;
    // Nicht-interaktiv: Host-Key bei Erstkontakt automatisch akzeptieren (TOFU) —
    // ein interaktiver yes/no-Prompt würde den Browser sonst hängen lassen.
    args << QStringLiteral("-o") << QStringLiteral("StrictHostKeyChecking=accept-new");
    args << (user.isEmpty() ? host : user + QLatin1Char('@') + host);

    m_cmd = Cmd::Connect;
    m_buf.clear();
    setBusy(true);
    setStatus(tr("Verbinde mit %1 …").arg(host));
    if (!m_pty.start(QStringLiteral("sftp"), args, 80, 24)) {
        setBusy(false);
        emit error(tr("sftp konnte nicht gestartet werden."));
        return;
    }
    // Sicherheitsnetz gegen hängende Verbindungen.
    QTimer::singleShot(25000, this, [this]() {
        if (!m_connected && m_pty.isRunning()) {
            close();
            emit error(tr("Zeitüberschreitung beim Verbindungsaufbau."));
        }
    });
}

void SftpClient::startCommand(Cmd c, const QByteArray &line) {
    m_cmd = c;
    m_buf.clear();
    setBusy(true);
    if (!line.isEmpty()) m_pty.write(line + '\n');
}

void SftpClient::onData(const QByteArray &data) {
    m_buf.append(data);
    if (m_buf.size() > 1 << 20) m_buf = m_buf.right(1 << 20);   // Wildwuchs kappen

    // Passwortabfrage automatisch beantworten (einmalig).
    if (m_passwordPending && m_buf.toLower().contains("password:")) {
        m_passwordPending = false;
        m_pty.write(m_password.toUtf8() + '\n');
        m_password.clear();
        m_buf.clear();
        return;
    }
    // Ein Befehl gilt als fertig, sobald der nächste sftp>-Prompt erscheint.
    if (m_buf.contains("sftp> ")) handlePrompt();
}

void SftpClient::handlePrompt() {
    const QString out = QString::fromUtf8(m_buf);
    const QString low = out.toLower();
    auto hasError = [&]() {
        return low.contains(QStringLiteral("no such file"))
            || low.contains(QStringLiteral("not found"))
            || low.contains(QStringLiteral("permission denied"))
            || low.contains(QStringLiteral("couldn't"))
            || low.contains(QStringLiteral("can't"));
    };

    switch (m_cmd) {
    case Cmd::Connect:
        m_connected = true;
        emit connectedChanged();
        setStatus(tr("Verbunden."));
        startCommand(Cmd::Pwd, "pwd");
        return;
    case Cmd::Pwd: {
        // "Remote working directory: /home/user"
        const int i = out.indexOf(QStringLiteral("working directory:"));
        if (i >= 0) {
            QString path = out.mid(i + 18).trimmed();
            const int nl = path.indexOf(QLatin1Char('\n'));
            if (nl >= 0) path = path.left(nl);
            m_currentPath = path.trimmed();
        }
        if (m_currentPath.isEmpty()) m_currentPath = QStringLiteral("/");
        emit currentPathChanged();
        startCommand(Cmd::List, "ls -la");
        return;
    }
    case Cmd::List:
        m_entries = parseListing(out);
        emit entriesChanged();
        setStatus(tr("%n Einträge", "", int(m_entries.size())));
        break;
    case Cmd::Cd:
        if (hasError()) {
            emit error(tr("Verzeichniswechsel fehlgeschlagen: %1").arg(m_pendingPath));
        } else {
            m_currentPath = m_pendingPath;
            emit currentPathChanged();
            startCommand(Cmd::List, "ls -la");
            return;
        }
        break;
    case Cmd::Download:
        if (hasError()) emit transferFinished(false, tr("Download fehlgeschlagen."));
        else { emit transferFinished(true, tr("Heruntergeladen: %1").arg(m_pendingPath));
               setStatus(tr("Heruntergeladen: %1").arg(m_pendingPath)); }
        break;
    case Cmd::Upload:
        if (hasError()) emit transferFinished(false, tr("Upload fehlgeschlagen."));
        else { emit transferFinished(true, tr("Hochgeladen: %1").arg(m_pendingPath));
               startCommand(Cmd::List, "ls -la"); return; }   // Liste auffrischen
        break;
    case Cmd::None:
        break;
    }
    m_cmd = Cmd::None;
    setBusy(false);
}

void SftpClient::onFinished(int) {
    const bool wasConnected = m_connected;
    m_connected = false;
    m_passwordPending = false;
    m_cmd = Cmd::None;
    setBusy(false);
    emit connectedChanged();
    if (!wasConnected) {
        // Vor dem ersten Prompt beendet → Authentifizierung/Verbindung scheiterte.
        QString detail = QString::fromUtf8(m_buf).trimmed();
        const int nl = detail.lastIndexOf(QLatin1Char('\n'));
        if (nl >= 0) detail = detail.mid(nl + 1).trimmed();
        emit error(detail.isEmpty() ? tr("Verbindung fehlgeschlagen.") : detail);
    } else {
        setStatus(tr("Verbindung geschlossen."));
    }
}

void SftpClient::refresh() {
    if (m_connected && !m_busy) startCommand(Cmd::List, "ls -la");
}

void SftpClient::cd(const QString &name) {
    if (!m_connected || m_busy || name.isEmpty()) return;
    m_pendingPath = remoteJoin(m_currentPath, name);
    startCommand(Cmd::Cd, "cd \"" + m_pendingPath.toUtf8() + "\"");
}

void SftpClient::cdUp() {
    if (!m_connected || m_busy) return;
    QString p = m_currentPath;
    while (p.endsWith(QLatin1Char('/')) && p.size() > 1) p.chop(1);
    const int slash = p.lastIndexOf(QLatin1Char('/'));
    QString parent = slash <= 0 ? QStringLiteral("/") : p.left(slash);
    if (parent == m_currentPath) return;
    m_pendingPath = parent;
    startCommand(Cmd::Cd, "cd \"" + parent.toUtf8() + "\"");
}

void SftpClient::download(const QString &name, const QString &localDir) {
    if (!m_connected || m_busy || name.isEmpty()) return;
    QString dir = localDir;
    if (dir.startsWith(QStringLiteral("file://"))) dir = QUrl(dir).toLocalFile();
    const QString remote = remoteJoin(m_currentPath, name);
    QString local = dir;
    if (!local.endsWith(QLatin1Char('/'))) local += QLatin1Char('/');
    local += name;
    m_pendingPath = local;
    setStatus(tr("Lade herunter: %1 …").arg(name));
    startCommand(Cmd::Download,
                 "get \"" + remote.toUtf8() + "\" \"" + local.toUtf8() + "\"");
}

void SftpClient::upload(const QString &localPath) {
    if (!m_connected || m_busy || localPath.isEmpty()) return;
    QString local = localPath;
    if (local.startsWith(QStringLiteral("file://"))) local = QUrl(local).toLocalFile();
    const QString base = QFileInfo(local).fileName();
    const QString remote = remoteJoin(m_currentPath, base);
    m_pendingPath = remote;
    setStatus(tr("Lade hoch: %1 …").arg(base));
    startCommand(Cmd::Upload,
                 "put \"" + local.toUtf8() + "\" \"" + remote.toUtf8() + "\"");
}

void SftpClient::close() {
    if (m_pty.isRunning()) {
        m_pty.write("bye\n");   // höflich beenden …
        m_pty.terminate();      // … und sicher aufräumen
    }
    m_cmd = Cmd::None;
    m_passwordPending = false;
    if (m_connected) { m_connected = false; emit connectedChanged(); }
    setBusy(false);
}

// --- ls -la-Parsing (statisch, Gui-frei, in tst_sftp testbar) ----------------
QVariantList SftpClient::parseListing(const QString &output) {
    QVariantList out;
    const QStringList lines = output.split(QLatin1Char('\n'));
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty()) continue;
        // Erstes Zeichen muss ein gültiger Dateityp sein (Perms-Block), sonst keine
        // Listenzeile (Echo „ls -la", „sftp> ", Fehlertexte werden so übersprungen).
        const QChar t = line.at(0);
        if (t != QLatin1Char('d') && t != QLatin1Char('-') && t != QLatin1Char('l')
            && t != QLatin1Char('b') && t != QLatin1Char('c') && t != QLatin1Char('p')
            && t != QLatin1Char('s'))
            continue;
        const QStringList tok = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (tok.size() < 9) continue;
        if (tok.first().size() < 10) continue;       // Perms-Block "drwxr-xr-x"
        bool ok = false;
        const qlonglong size = tok.at(4).toLongLong(&ok);
        if (!ok) continue;                            // Feld 4 ist die Größe
        QString name = QStringList(tok.mid(8)).join(QLatin1Char(' '));
        const bool isLink = (t == QLatin1Char('l'));
        if (isLink) {                                 // "name -> target" → nur Name
            const int arrow = name.indexOf(QStringLiteral(" -> "));
            if (arrow >= 0) name = name.left(arrow);
        }
        if (name == QStringLiteral(".") || name == QStringLiteral("..")) continue;
        QVariantMap e;
        e[QStringLiteral("name")] = name;
        e[QStringLiteral("size")] = size;
        e[QStringLiteral("isDir")] = (t == QLatin1Char('d'));
        e[QStringLiteral("isLink")] = isLink;
        out.append(e);
    }
    return out;
}

} // namespace qtmux

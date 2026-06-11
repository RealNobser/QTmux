#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <qqmlintegration.h>

#include "Pty.h"

namespace qtmux {

/// SFTP-Client für den Remote-Datei-Browser (QTMUX-7-Rest). Treibt den **System-
/// `sftp`-Client** interaktiv über den bestehenden PTY-Layer — kein libssh2/keine
/// Krypto-Abhängigkeit, konsistent mit dem System-`ssh`-Ansatz von SshBackend.
/// Auth (Key/Agent/known_hosts) „funktioniert einfach"; ein Passwort kann (z. B. aus
/// dem Vault) mitgegeben und automatisch an die Abfrage gesendet werden.
///
/// Steuerung: jeder Befehl wird ans PTY geschrieben, die Antwort bis zum nächsten
/// `sftp> `-Prompt gesammelt und ausgewertet. Verzeichnislisten werden aus `ls -la`
/// geparst (siehe parseListing — Gui-frei testbar).
class SftpClient : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentPathChanged)
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
public:
    explicit SftpClient(QObject *parent = nullptr);
    ~SftpClient() override;

    bool isConnected() const { return m_connected; }
    bool isBusy() const { return m_busy; }
    QString currentPath() const { return m_currentPath; }
    QVariantList entries() const { return m_entries; }
    QString status() const { return m_status; }

    /// Baut die SFTP-Verbindung auf. `password` (optional) wird automatisch an die
    /// Passwortabfrage gesendet (leer = Key/Agent). Bei Erfolg folgt eine Auflistung
    /// des Heimatverzeichnisses.
    Q_INVOKABLE void connectTo(const QString &host, int port, const QString &user,
                               const QString &identity, const QString &password);
    /// Aktuelles Verzeichnis erneut auflisten.
    Q_INVOKABLE void refresh();
    /// In ein Unterverzeichnis wechseln (relativ zum aktuellen Pfad).
    Q_INVOKABLE void cd(const QString &name);
    /// Eine Ebene nach oben.
    Q_INVOKABLE void cdUp();
    /// Datei `name` (im aktuellen Verzeichnis) nach `localDir` herunterladen.
    Q_INVOKABLE void download(const QString &name, const QString &localDir);
    /// Lokale Datei `localPath` ins aktuelle Verzeichnis hochladen.
    Q_INVOKABLE void upload(const QString &localPath);
    /// Verbindung schließen/abbrechen.
    Q_INVOKABLE void close();

    /// Parst eine `ls -la`-Ausgabe (inkl. evtl. Echo-/Prompt-/Fehlerzeilen, die
    /// übersprungen werden) in eine Liste von {name, size, isDir, isLink}-Maps.
    /// Gui-frei und statisch → in tst_sftp ohne Server testbar.
    static QVariantList parseListing(const QString &output);

signals:
    void connectedChanged();
    void busyChanged();
    void currentPathChanged();
    void entriesChanged();
    void statusChanged();
    /// Verbindungs-/Befehlsfehler (Text für die UI).
    void error(const QString &message);
    /// Ergebnis eines Down-/Uploads.
    void transferFinished(bool ok, const QString &message);

private:
    enum class Cmd { None, Connect, Pwd, List, Cd, Download, Upload };

    void onData(const QByteArray &data);
    void onFinished(int exitCode);
    void startCommand(Cmd c, const QByteArray &line);   // Befehl senden (leer = nur warten)
    void handlePrompt();                                // sftp>-Prompt erreicht → auswerten
    void setBusy(bool b);
    void setStatus(const QString &s);
    static QString remoteJoin(const QString &base, const QString &name);

    Pty m_pty;
    QByteArray m_buf;          // Ausgabe seit Befehlsbeginn
    Cmd m_cmd = Cmd::None;
    QString m_currentPath;
    QString m_pendingPath;     // Ziel-/Hilfspfad des laufenden Befehls
    QVariantList m_entries;
    QString m_status;
    QString m_password;
    bool m_passwordPending = false;
    bool m_connected = false;
    bool m_busy = false;
};

} // namespace qtmux

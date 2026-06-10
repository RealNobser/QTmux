#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QList>

namespace qtmux {

/// Eine gespeicherte Verbindungs-/Session-Vorlage (Connection-Manager, QTMUX-7).
/// `type` folgt den qtmux::Session::Type-Ordinalwerten: 0=Shell, 1=Ssh, 2=Serial.
struct ConnectionProfile {
    QString name;
    int type = 0;
    // SSH
    QString host;
    int port = 22;
    QString user;
    QString identity;
    // Shell
    QString program;      // leer = Standard-Shell
    QString workingDir;
    // Seriell
    QString serialPort;
    int baud = 115200;
};

/// Prozessweite Verwaltung gespeicherter Verbindungsprofile (Gui-frei, nur Qt Core).
/// Von QML über den in main.cpp registrierten Context-Property `Profiles` genutzt;
/// via QSettings persistiert. Das eigentliche Starten übernimmt SessionModel (QML
/// liest das Profil und ruft create{Shell,Ssh,Serial}Session) — die Registry kennt
/// keine Sessions und bleibt dadurch testbar und entkoppelt.
class ConnectionProfileRegistry : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList profiles READ profilesVariant NOTIFY changed)
public:
    /// Prozessweite Instanz (geteilt zwischen Core und QML).
    static ConnectionProfileRegistry *instance();

    QVariantList profilesVariant() const;
    const QList<ConnectionProfile> &profiles() const { return m_profiles; }

    /// Profil nach Name als Map (leer, wenn unbekannt).
    Q_INVOKABLE QVariantMap profile(const QString &name) const;
    /// Legt ein Profil an oder aktualisiert das gleichnamige (Upsert über `name`).
    Q_INVOKABLE void saveProfile(const QVariantMap &data);
    /// Entfernt das Profil mit diesem Namen.
    Q_INVOKABLE void removeProfile(const QString &name);

signals:
    void changed();

private:
    explicit ConnectionProfileRegistry(QObject *parent = nullptr);
    void load();
    void persist() const;
    int indexOf(const QString &name) const;

    static ConnectionProfile fromMap(const QVariantMap &m);
    static QVariantMap toMap(const ConnectionProfile &p);

    QList<ConnectionProfile> m_profiles;
};

} // namespace qtmux

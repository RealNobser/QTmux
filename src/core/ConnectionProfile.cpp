#include "ConnectionProfile.h"

#include <QSettings>

namespace qtmux {

ConnectionProfileRegistry *ConnectionProfileRegistry::instance() {
    static ConnectionProfileRegistry reg;
    return &reg;
}

ConnectionProfileRegistry::ConnectionProfileRegistry(QObject *parent) : QObject(parent) {
    load();
}

int ConnectionProfileRegistry::indexOf(const QString &name) const {
    for (int i = 0; i < m_profiles.size(); ++i)
        if (m_profiles.at(i).name == name) return i;
    return -1;
}

ConnectionProfile ConnectionProfileRegistry::fromMap(const QVariantMap &m) {
    ConnectionProfile p;
    p.name       = m.value(QStringLiteral("name")).toString().trimmed();
    p.type       = m.value(QStringLiteral("type")).toInt();
    p.host       = m.value(QStringLiteral("host")).toString();
    p.port       = m.value(QStringLiteral("port"), 22).toInt();
    p.user       = m.value(QStringLiteral("user")).toString();
    p.identity   = m.value(QStringLiteral("identity")).toString();
    p.program    = m.value(QStringLiteral("program")).toString();
    p.workingDir = m.value(QStringLiteral("workingDir")).toString();
    p.serialPort = m.value(QStringLiteral("serialPort")).toString();
    p.baud       = m.value(QStringLiteral("baud"), 115200).toInt();
    return p;
}

QVariantMap ConnectionProfileRegistry::toMap(const ConnectionProfile &p) {
    QVariantMap m;
    m[QStringLiteral("name")]       = p.name;
    m[QStringLiteral("type")]       = p.type;
    m[QStringLiteral("host")]       = p.host;
    m[QStringLiteral("port")]       = p.port;
    m[QStringLiteral("user")]       = p.user;
    m[QStringLiteral("identity")]   = p.identity;
    m[QStringLiteral("program")]    = p.program;
    m[QStringLiteral("workingDir")] = p.workingDir;
    m[QStringLiteral("serialPort")] = p.serialPort;
    m[QStringLiteral("baud")]       = p.baud;
    return m;
}

QVariantList ConnectionProfileRegistry::profilesVariant() const {
    QVariantList out;
    for (const ConnectionProfile &p : m_profiles) out.append(toMap(p));
    return out;
}

QVariantMap ConnectionProfileRegistry::profile(const QString &name) const {
    const int i = indexOf(name);
    return i >= 0 ? toMap(m_profiles.at(i)) : QVariantMap{};
}

void ConnectionProfileRegistry::saveProfile(const QVariantMap &data) {
    ConnectionProfile p = fromMap(data);
    if (p.name.isEmpty()) return;   // namenlose Profile werden ignoriert
    const int i = indexOf(p.name);
    if (i >= 0) m_profiles[i] = p;  // Upsert: gleichnamiges Profil ersetzen
    else m_profiles.append(p);
    persist();
    emit changed();
}

void ConnectionProfileRegistry::removeProfile(const QString &name) {
    const int i = indexOf(name);
    if (i < 0) return;
    m_profiles.removeAt(i);
    persist();
    emit changed();
}

void ConnectionProfileRegistry::load() {
    QSettings s;
    const int n = s.beginReadArray(QStringLiteral("profiles"));
    m_profiles.clear();
    for (int i = 0; i < n; ++i) {
        s.setArrayIndex(i);
        ConnectionProfile p;
        p.name       = s.value(QStringLiteral("name")).toString();
        p.type       = s.value(QStringLiteral("type")).toInt();
        p.host       = s.value(QStringLiteral("host")).toString();
        p.port       = s.value(QStringLiteral("port"), 22).toInt();
        p.user       = s.value(QStringLiteral("user")).toString();
        p.identity   = s.value(QStringLiteral("identity")).toString();
        p.program    = s.value(QStringLiteral("program")).toString();
        p.workingDir = s.value(QStringLiteral("workingDir")).toString();
        p.serialPort = s.value(QStringLiteral("serialPort")).toString();
        p.baud       = s.value(QStringLiteral("baud"), 115200).toInt();
        if (!p.name.isEmpty()) m_profiles.append(p);
    }
    s.endArray();
}

void ConnectionProfileRegistry::persist() const {
    QSettings s;
    s.beginWriteArray(QStringLiteral("profiles"), m_profiles.size());
    for (int i = 0; i < m_profiles.size(); ++i) {
        const ConnectionProfile &p = m_profiles.at(i);
        s.setArrayIndex(i);
        s.setValue(QStringLiteral("name"), p.name);
        s.setValue(QStringLiteral("type"), p.type);
        s.setValue(QStringLiteral("host"), p.host);
        s.setValue(QStringLiteral("port"), p.port);
        s.setValue(QStringLiteral("user"), p.user);
        s.setValue(QStringLiteral("identity"), p.identity);
        s.setValue(QStringLiteral("program"), p.program);
        s.setValue(QStringLiteral("workingDir"), p.workingDir);
        s.setValue(QStringLiteral("serialPort"), p.serialPort);
        s.setValue(QStringLiteral("baud"), p.baud);
    }
    s.endArray();
}

} // namespace qtmux

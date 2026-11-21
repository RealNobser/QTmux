#include "update/UpdateSettings.hpp"

#include "update/UpdatePolicy.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

#include <limits>

namespace appupdate {

namespace k = settingskey;

// The notes travel in the marker only for state 6; the parser caps them at
// 64 Ki characters anyway (ReleaseNotes.hpp).
constexpr int kNotesMarkerMaxChars = 64 * 1024;

namespace {

QString key(const char* k) { return QString::fromLatin1(k); }

// QTmux' spellings until contract V2.
constexpr const char* kQAutoCheck  = "update/autoCheck";
constexpr const char* kQLastCheck  = "update/lastCheck";
constexpr const char* kQSkipped    = "update/skippedVersion";
constexpr const char* kQProxyMode  = "update/proxyMode";
constexpr const char* kQProxyType  = "update/proxyType";
constexpr const char* kQProxyHost  = "update/proxyHost";
constexpr const char* kQProxyPort  = "update/proxyPort";
constexpr const char* kQProxyUser  = "update/proxyUser";

// Moves `from` onto `to` through `convert` unless `to` is already set; the
// old key goes in both cases.
template <typename Convert>
bool carry(QVariantMap& m, const char* from, const char* to, Convert convert)
{
    const QString f = key(from);
    if (!m.contains(f)) return false;
    const QString t = key(to);
    if (!m.contains(t)) {
        const QVariant v = convert(m.value(f));
        if (v.isValid()) m.insert(t, v);
    }
    m.remove(f);
    return true;
}

QVariant same(const QVariant& v) { return v; }

}  // namespace

// ---- Words ----------------------------------------------------------------

QString UpdateSettings::proxyModeWord(ProxyConfig::Mode m)
{
    switch (m) {
    case ProxyConfig::Mode::System: return QStringLiteral("system");
    case ProxyConfig::Mode::None:   return QStringLiteral("none");
    case ProxyConfig::Mode::Manual: return QStringLiteral("manual");
    }
    return QStringLiteral("system");
}

ProxyConfig::Mode UpdateSettings::proxyModeFromWord(const QString& w)
{
    if (w == QLatin1String("none"))   return ProxyConfig::Mode::None;
    if (w == QLatin1String("manual")) return ProxyConfig::Mode::Manual;
    return ProxyConfig::Mode::System;
}

// ---- Pure migration ---------------------------------------------------------

bool UpdateSettings::migrateLegacyKeys(QVariantMap& m)
{
    bool changed = false;
    changed |= carry(m, kQAutoCheck, k::kAutoCheck, same);
    changed |= carry(m, kQLastCheck, k::kLastCheck, same);
    changed |= carry(m, kQSkipped, k::kSkippedVersion, same);
    changed |= carry(m, kQProxyMode, k::kProxyMode, [](const QVariant& v) {
        bool ok = false;
        const int n = v.toInt(&ok);
        if (!ok) return QVariant();
        // QTmux: 0 System, 1 None, 2 Manual (UpdateViewModel::proxyConfig).
        switch (n) {
        case 1:  return QVariant(QStringLiteral("none"));
        case 2:  return QVariant(QStringLiteral("manual"));
        default: return QVariant(QStringLiteral("system"));
        }
    });
    changed |= carry(m, kQProxyType, k::kProxyType, [](const QVariant& v) {
        return QVariant(v.toInt() == 1 ? QStringLiteral("socks5")
                                       : QStringLiteral("http"));
    });
    changed |= carry(m, kQProxyHost, k::kProxyHost, same);
    changed |= carry(m, kQProxyPort, k::kProxyPort, same);
    changed |= carry(m, kQProxyUser, k::kProxyUser, same);
    for (const char* dropped : {k::kBaseUrlDropped, k::kBaseUrlDroppedQtmux}) {
        if (m.remove(key(dropped)) > 0) changed = true;
    }
    return changed;
}

bool UpdateSettings::isExportable(const QString& key)
{
    if (key.startsWith(QLatin1String(k::kLastSerialPrefix))) return false;
    for (const char* never : {k::kProxyUser, k::kPendingInstall, k::kInstallBlockedCode,
                              k::kBaseUrlDropped, k::kBaseUrlDroppedQtmux,
                              kQProxyUser}) {
        if (key == QLatin1String(never)) return false;
    }
    return true;
}

// ---- Live settings ----------------------------------------------------------

UpdateSettings::UpdateSettings(QSettings& settings) : m_s(settings) {}

bool UpdateSettings::migrate()
{
    // Snapshot only the update/ group: the migration knows nothing else, and
    // touching foreign keys would be a side effect on the app's settings.
    QVariantMap snap;
    const QStringList all = m_s.allKeys();
    for (const QString& kk : all) {
        if (kk.startsWith(QLatin1String("update/"))) snap.insert(kk, m_s.value(kk));
    }
    QVariantMap after = snap;
    if (!migrateLegacyKeys(after)) return false;
    for (auto it = snap.constBegin(); it != snap.constEnd(); ++it) {
        if (!after.contains(it.key())) m_s.remove(it.key());
    }
    for (auto it = after.constBegin(); it != after.constEnd(); ++it) {
        if (!snap.contains(it.key()) || snap.value(it.key()) != it.value()) {
            m_s.setValue(it.key(), it.value());
        }
    }
    m_s.sync();
    return true;
}

bool UpdateSettings::autoCheck() const
{
    return m_s.value(key(k::kAutoCheck), true).toBool();
}
void UpdateSettings::setAutoCheck(bool on) { m_s.setValue(key(k::kAutoCheck), on); }

QDateTime UpdateSettings::lastCheck() const
{
    return QDateTime::fromString(m_s.value(key(k::kLastCheck)).toString(), Qt::ISODate);
}
void UpdateSettings::setLastCheck(const QDateTime& when)
{
    m_s.setValue(key(k::kLastCheck), when.toUTC().toString(Qt::ISODate));
}

QString UpdateSettings::skippedVersion() const
{
    return m_s.value(key(k::kSkippedVersion)).toString();
}
void UpdateSettings::setSkippedVersion(const QString& version)
{
    m_s.setValue(key(k::kSkippedVersion), version);
}
void UpdateSettings::clearSkippedVersion() { m_s.remove(key(k::kSkippedVersion)); }

bool UpdateSettings::dryRunLaunch() const
{
    return m_s.value(key(k::kDryRunLaunch), false).toBool();
}

ProxyConfig UpdateSettings::proxyConfig() const
{
    ProxyConfig c;
    c.mode = proxyModeFromWord(m_s.value(key(k::kProxyMode)).toString());
    c.type = m_s.value(key(k::kProxyType)).toString() == QLatin1String("socks5")
                 ? QNetworkProxy::Socks5Proxy
                 : QNetworkProxy::HttpProxy;
    c.host = m_s.value(key(k::kProxyHost)).toString().trimmed();
    bool ok = false;
    const int port = m_s.value(key(k::kProxyPort), 0).toInt(&ok);
    c.port = (ok && port > 0 && port <= 65535) ? static_cast<quint16>(port) : 0;
    c.user = m_s.value(key(k::kProxyUser)).toString();
    return c;
}

void UpdateSettings::setProxyConfig(const ProxyConfig& c)
{
    m_s.setValue(key(k::kProxyMode), proxyModeWord(c.mode));
    m_s.setValue(key(k::kProxyType), c.type == QNetworkProxy::Socks5Proxy
                                         ? QStringLiteral("socks5")
                                         : QStringLiteral("http"));
    m_s.setValue(key(k::kProxyHost), c.host);
    m_s.setValue(key(k::kProxyPort), static_cast<int>(c.port));
    m_s.setValue(key(k::kProxyUser), c.user);
}

SerialState UpdateSettings::serialState(const QString& productSlug) const
{
    const QString kk = policy::lastSerialSettingsKey(productSlug);
    if (!m_s.contains(kk)) return SerialState::Absent;
    bool ok = false;
    const qulonglong v = m_s.value(kk).toULongLong(&ok);
    if (!ok || v == 0 || v > std::numeric_limits<quint32>::max()) {
        return SerialState::Unreadable;
    }
    return SerialState::Valid;
}

SerialStore UpdateSettings::serialStore() const
{
    QSettings* s = &m_s;
    SerialStore store;
    store.load = [s](const QString& slug) -> quint32 {
        const QString kk = policy::lastSerialSettingsKey(slug);
        bool ok = false;
        const qulonglong v = s->value(kk).toULongLong(&ok);
        if (!ok || v == 0 || v > std::numeric_limits<quint32>::max()) return 0;
        return static_cast<quint32>(v);
    };
    store.store = [s](const QString& slug, quint32 serial) {
        const QString kk = policy::lastSerialSettingsKey(slug);
        bool ok = false;
        const qulonglong old = s->value(kk).toULongLong(&ok);
        const bool oldValid = ok && old > 0 && old <= std::numeric_limits<quint32>::max();
        if (oldValid && serial <= old) return;  // never moves backwards
        s->setValue(kk, static_cast<qulonglong>(serial));
        s->sync();
    };
    return store;
}

PendingInstall UpdateSettings::pendingInstall() const
{
    PendingInstall p;
    const QByteArray raw = m_s.value(key(k::kPendingInstall)).toString().toUtf8();
    if (raw.isEmpty() || raw.size() > 256 * 1024) return p;
    const QJsonObject o = QJsonDocument::fromJson(raw).object();
    p.fromVersion = o.value(QLatin1String("from")).toString();
    p.toVersion   = o.value(QLatin1String("to")).toString();
    p.method      = o.value(QLatin1String("method")).toString();
    p.file        = o.value(QLatin1String("file")).toString();
    p.sha256      = o.value(QLatin1String("sha256")).toString();
    p.size        = static_cast<qint64>(o.value(QLatin1String("size")).toDouble());
    p.kind        = o.value(QLatin1String("kind")).toString();
    p.resultFile  = o.value(QLatin1String("result_file")).toString();
    p.confirmFile = o.value(QLatin1String("confirm_file")).toString();
    p.attempt     = o.value(QLatin1String("attempt")).toString();
    p.launchError = o.value(QLatin1String("launch_error")).toString();
    p.notes       = o.value(QLatin1String("notes")).toString();
    p.published   = o.value(QLatin1String("published")).toString();
    p.startedUtc  = QDateTime::fromString(o.value(QLatin1String("started")).toString(),
                                          Qt::ISODate);
    return p;
}

void UpdateSettings::setPendingInstall(const PendingInstall& p)
{
    QJsonObject o;
    o.insert(QLatin1String("from"), p.fromVersion);
    o.insert(QLatin1String("to"), p.toVersion);
    o.insert(QLatin1String("method"), p.method);
    o.insert(QLatin1String("file"), p.file);
    o.insert(QLatin1String("sha256"), p.sha256);
    o.insert(QLatin1String("size"), static_cast<double>(p.size));
    o.insert(QLatin1String("kind"), p.kind);
    o.insert(QLatin1String("result_file"), p.resultFile);
    o.insert(QLatin1String("confirm_file"), p.confirmFile);
    if (!p.attempt.isEmpty()) o.insert(QLatin1String("attempt"), p.attempt);
    if (!p.launchError.isEmpty()) o.insert(QLatin1String("launch_error"), p.launchError);
    o.insert(QLatin1String("notes"), p.notes.left(kNotesMarkerMaxChars));
    o.insert(QLatin1String("published"), p.published);
    o.insert(QLatin1String("started"), p.startedUtc.toUTC().toString(Qt::ISODate));
    m_s.setValue(key(k::kPendingInstall),
                 QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));
    // The app is about to quit — the marker must be on disk, not in a cache.
    m_s.sync();
}

void UpdateSettings::clearPendingInstall()
{
    m_s.remove(key(k::kPendingInstall));
    m_s.sync();
}

QString UpdateSettings::installBlockedCode() const
{
    return m_s.value(key(k::kInstallBlockedCode)).toString().left(64);
}

void UpdateSettings::setInstallBlockedCode(const QString& code)
{
    if (code.isEmpty()) m_s.remove(key(k::kInstallBlockedCode));
    else m_s.setValue(key(k::kInstallBlockedCode), code.left(64));
}

}  // namespace appupdate

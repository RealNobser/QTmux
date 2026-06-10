#include "SecretsVault.h"

#include <QMessageAuthenticationCode>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace qtmux {

namespace {
constexpr int kSaltLen = 16;
constexpr int kNonceLen = 16;
constexpr int kTagLen = 32;     // HMAC-SHA256
constexpr int kVersion = 1;

// PBKDF2-HMAC-SHA512 (RFC 2898) auf Qt's HMAC. Bewusst selbst implementiert, damit
// qtmux_core nur Qt6::Core braucht (QPasswordDigestor lebt in QtNetwork). PBKDF2 ist
// ein eindeutiger Standard — das Sicherheits-Primitiv bleibt Qt's HMAC-SHA512.
QByteArray pbkdf2HmacSha512(const QByteArray &password, const QByteArray &salt,
                            int iterations, int dkLen) {
    constexpr int hLen = 64;   // SHA-512
    QByteArray dk;
    const int blocks = (dkLen + hLen - 1) / hLen;
    for (int i = 1; i <= blocks; ++i) {
        QByteArray intI(4, 0);
        intI[0] = static_cast<char>((i >> 24) & 0xff);
        intI[1] = static_cast<char>((i >> 16) & 0xff);
        intI[2] = static_cast<char>((i >> 8) & 0xff);
        intI[3] = static_cast<char>(i & 0xff);
        QByteArray u = QMessageAuthenticationCode::hash(salt + intI, password,
                                                        QCryptographicHash::Sha512);
        QByteArray t = u;
        for (int j = 1; j < iterations; ++j) {
            u = QMessageAuthenticationCode::hash(u, password, QCryptographicHash::Sha512);
            for (int k = 0; k < t.size(); ++k)
                t[k] = t[k] ^ u[k];
        }
        dk += t;
    }
    return dk.left(dkLen);
}
}

SecretsVault *SecretsVault::instance() {
    static SecretsVault v;
    return &v;
}

SecretsVault::SecretsVault(QObject *parent) : QObject(parent) {}

QString SecretsVault::filePath() const {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dir + QStringLiteral("/vault.json");
}

bool SecretsVault::exists() const {
    return QFile::exists(filePath());
}

QStringList SecretsVault::secretNames() const {
    return m_unlocked ? QStringList(m_secrets.keys()) : QStringList();
}

// --- Krypto-Hilfen ----------------------------------------------------------

QByteArray SecretsVault::randomBytes(int n) {
    QByteArray b(n, 0);
    QRandomGenerator::system()->fillRange(
        reinterpret_cast<quint32 *>(b.data()), n / 4);
    // Restbytes (n nicht durch 4 teilbar) auffüllen.
    for (int i = (n / 4) * 4; i < n; ++i)
        b[i] = static_cast<char>(QRandomGenerator::system()->bounded(256));
    return b;
}

void SecretsVault::deriveKeys(const QString &password, const QByteArray &salt, int iter,
                              QByteArray &encKey, QByteArray &macKey) {
    // 64 Byte Schlüsselmaterial → 32 Byte encKey + 32 Byte macKey.
    const QByteArray master = pbkdf2HmacSha512(password.toUtf8(), salt, iter, 64);
    encKey = master.left(32);
    macKey = master.mid(32, 32);
}

QByteArray SecretsVault::keystreamXor(const QByteArray &data, const QByteArray &encKey,
                                      const QByteArray &nonce) {
    // CTR-Keystream: block_i = HMAC-SHA256(encKey, nonce || be64(i)); XOR mit data.
    QByteArray out(data.size(), 0);
    int offset = 0;
    quint64 block = 0;
    while (offset < data.size()) {
        QMessageAuthenticationCode mac(QCryptographicHash::Sha256, encKey);
        mac.addData(nonce);
        QByteArray ctr(8, 0);
        quint64 b = block;
        for (int i = 7; i >= 0; --i) { ctr[i] = static_cast<char>(b & 0xff); b >>= 8; }
        mac.addData(ctr);
        const QByteArray ks = mac.result();          // 32 Byte Schlüsselblock
        const int n = qMin(ks.size(), data.size() - offset);
        for (int i = 0; i < n; ++i)
            out[offset + i] = data[offset + i] ^ ks[i];
        offset += n;
        ++block;
    }
    return out;
}

QByteArray SecretsVault::computeTag(const QByteArray &macKey, const QByteArray &salt,
                                    const QByteArray &nonce, const QByteArray &ct) {
    QMessageAuthenticationCode mac(QCryptographicHash::Sha256, macKey);
    mac.addData(salt);
    mac.addData(nonce);
    mac.addData(ct);
    return mac.result();
}

bool SecretsVault::constTimeEquals(const QByteArray &a, const QByteArray &b) {
    if (a.size() != b.size()) return false;
    quint8 diff = 0;
    for (int i = 0; i < a.size(); ++i)
        diff |= static_cast<quint8>(a[i]) ^ static_cast<quint8>(b[i]);
    return diff == 0;
}

// --- Persistenz -------------------------------------------------------------

bool SecretsVault::persist() {
    if (!m_unlocked) return false;
    // Geheimnisse als JSON-Objekt serialisieren.
    QJsonObject obj;
    for (auto it = m_secrets.constBegin(); it != m_secrets.constEnd(); ++it)
        obj.insert(it.key(), it.value());
    const QByteArray plain = QJsonDocument(obj).toJson(QJsonDocument::Compact);

    const QByteArray nonce = randomBytes(kNonceLen);
    const QByteArray ct = keystreamXor(plain, m_encKey, nonce);
    const QByteArray tag = computeTag(m_macKey, m_salt, nonce, ct);

    QJsonObject file;
    file.insert(QStringLiteral("v"), kVersion);
    file.insert(QStringLiteral("kdf"), QStringLiteral("pbkdf2-sha512"));
    file.insert(QStringLiteral("iter"), m_iter);
    file.insert(QStringLiteral("salt"), QString::fromLatin1(m_salt.toBase64()));
    file.insert(QStringLiteral("nonce"), QString::fromLatin1(nonce.toBase64()));
    file.insert(QStringLiteral("ct"), QString::fromLatin1(ct.toBase64()));
    file.insert(QStringLiteral("tag"), QString::fromLatin1(tag.toBase64()));

    const QString path = filePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QJsonDocument(file).toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

// --- Öffentliche API --------------------------------------------------------

bool SecretsVault::create(const QString &masterPassword) {
    if (exists() || masterPassword.isEmpty()) return false;
    m_iter = 210000;
    m_salt = randomBytes(kSaltLen);
    deriveKeys(masterPassword, m_salt, m_iter, m_encKey, m_macKey);
    m_secrets.clear();
    m_unlocked = true;
    if (!persist()) { lock(); return false; }
    emit lockedChanged();
    emit changed();
    return true;
}

bool SecretsVault::unlock(const QString &masterPassword) {
    QFile f(filePath());
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QJsonObject file = QJsonDocument::fromJson(f.readAll()).object();
    f.close();

    const QByteArray salt = QByteArray::fromBase64(file.value(QStringLiteral("salt")).toString().toLatin1());
    const QByteArray nonce = QByteArray::fromBase64(file.value(QStringLiteral("nonce")).toString().toLatin1());
    const QByteArray ct = QByteArray::fromBase64(file.value(QStringLiteral("ct")).toString().toLatin1());
    const QByteArray tag = QByteArray::fromBase64(file.value(QStringLiteral("tag")).toString().toLatin1());
    const int iter = file.value(QStringLiteral("iter")).toInt(210000);
    if (salt.isEmpty() || tag.size() != kTagLen) return false;

    QByteArray encKey, macKey;
    deriveKeys(masterPassword, salt, iter, encKey, macKey);

    // Authentifizieren (konstantzeitig) — falsches Passwort/Manipulation → Abbruch.
    if (!constTimeEquals(computeTag(macKey, salt, nonce, ct), tag)) {
        encKey.fill(0); macKey.fill(0);
        return false;
    }

    const QByteArray plain = keystreamXor(ct, encKey, nonce);
    const QJsonObject obj = QJsonDocument::fromJson(plain).object();
    m_secrets.clear();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
        m_secrets.insert(it.key(), it.value().toString());

    m_salt = salt;
    m_iter = iter;
    m_encKey = encKey;
    m_macKey = macKey;
    m_unlocked = true;
    emit lockedChanged();
    emit changed();
    return true;
}

void SecretsVault::lock() {
    if (!m_unlocked && m_secrets.isEmpty()) return;
    m_encKey.fill(0);
    m_macKey.fill(0);
    m_encKey.clear();
    m_macKey.clear();
    m_secrets.clear();
    const bool was = m_unlocked;
    m_unlocked = false;
    if (was) emit lockedChanged();
    emit changed();
}

bool SecretsVault::changeMasterPassword(const QString &oldPw, const QString &newPw) {
    if (!m_unlocked || newPw.isEmpty()) return false;
    // Altes Passwort verifizieren, indem wir die Schlüssel neu ableiten und mit den
    // aktuellen vergleichen (wir sind entsperrt, m_encKey/m_macKey sind die echten).
    QByteArray oe, om;
    deriveKeys(oldPw, m_salt, m_iter, oe, om);
    const bool ok = constTimeEquals(oe, m_encKey) && constTimeEquals(om, m_macKey);
    oe.fill(0); om.fill(0);
    if (!ok) return false;

    // Neues Salt + Schlüssel, dann neu verschlüsseln/persistieren.
    m_salt = randomBytes(kSaltLen);
    deriveKeys(newPw, m_salt, m_iter, m_encKey, m_macKey);
    return persist();
}

QString SecretsVault::secret(const QString &name) const {
    return m_unlocked ? m_secrets.value(name) : QString();
}

bool SecretsVault::hasSecret(const QString &name) const {
    return m_unlocked && m_secrets.contains(name);
}

bool SecretsVault::setSecret(const QString &name, const QString &value) {
    if (!m_unlocked || name.isEmpty()) return false;
    m_secrets.insert(name, value);
    if (!persist()) return false;
    emit changed();
    return true;
}

bool SecretsVault::removeSecret(const QString &name) {
    if (!m_unlocked) return false;
    if (m_secrets.remove(name) == 0) return false;
    if (!persist()) return false;
    emit changed();
    return true;
}

} // namespace qtmux

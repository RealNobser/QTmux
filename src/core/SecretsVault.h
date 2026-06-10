#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QMap>

namespace qtmux {

/// Verschlüsselter Secrets-Vault (QTMUX-22): speichert benannte Geheimnisse
/// (SSH-Passwörter/Passphrasen/Tokens) hinter einem Master-Passwort. Gui-frei
/// (nur Qt Core) und ohne externe Krypto-Abhängigkeit — die Konstruktion nutzt
/// ausschließlich Qt-Primitive:
///   - Schlüsselableitung: PBKDF2-HMAC-SHA512 (QPasswordDigestor), Zufalls-Salt,
///     hohe Iterationszahl → 64 Byte Schlüsselmaterial, gesplittet in encKey/macKey.
///   - Verschlüsselung: HMAC-SHA256 als PRF im Counter-Modus erzeugt einen Keystream,
///     der mit dem Klartext ge-XOR-t wird (frischer Zufalls-Nonce pro Schreibvorgang).
///   - Authentifizierung: Encrypt-then-MAC, tag = HMAC-SHA256(macKey, salt|nonce|ct).
///     Beim Entsperren wird der Tag (konstantzeitig) geprüft → falsches Passwort oder
///     Manipulation schlägt fehl.
/// Der gesamte Geheimnis-Satz wird als ein JSON-Blob ver-/entschlüsselt und in einer
/// Datei im App-Konfigverzeichnis abgelegt (vault.json). Schlüssel + Klartext liegen
/// nur im entsperrten Zustand im Speicher und werden beim Sperren überschrieben.
class SecretsVault : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool exists READ exists NOTIFY changed)
    Q_PROPERTY(bool unlocked READ isUnlocked NOTIFY lockedChanged)
    Q_PROPERTY(QStringList names READ secretNames NOTIFY changed)
public:
    /// Prozessweite Instanz (in main.cpp als Context-Property `Vault` registriert).
    static SecretsVault *instance();

    /// Existiert bereits eine Vault-Datei?
    bool exists() const;
    bool isUnlocked() const { return m_unlocked; }
    /// Namen aller Geheimnisse (nur sinnvoll im entsperrten Zustand, sonst leer).
    QStringList secretNames() const;

    /// Legt einen neuen, leeren Vault mit diesem Master-Passwort an (nur wenn keiner
    /// existiert) und entsperrt ihn. Gibt false bei vorhandenem Vault/leerem Passwort.
    Q_INVOKABLE bool create(const QString &masterPassword);
    /// Entsperrt den Vault. False bei falschem Passwort/fehlendem Vault.
    Q_INVOKABLE bool unlock(const QString &masterPassword);
    /// Sperrt den Vault: Schlüssel und Klartext werden aus dem Speicher gelöscht.
    Q_INVOKABLE void lock();
    /// Ändert das Master-Passwort (verlangt das alte). Vault wird neu verschlüsselt.
    Q_INVOKABLE bool changeMasterPassword(const QString &oldPw, const QString &newPw);

    /// Geheimnis lesen (leer, wenn gesperrt oder unbekannt).
    Q_INVOKABLE QString secret(const QString &name) const;
    Q_INVOKABLE bool hasSecret(const QString &name) const;
    /// Geheimnis anlegen/aktualisieren (Upsert). Erfordert entsperrten Vault; persistiert.
    Q_INVOKABLE bool setSecret(const QString &name, const QString &value);
    /// Geheimnis entfernen. Erfordert entsperrten Vault; persistiert.
    Q_INVOKABLE bool removeSecret(const QString &name);

signals:
    void changed();        // Geheimnis-Liste/Existenz geändert
    void lockedChanged();  // entsperrt/gesperrt

private:
    explicit SecretsVault(QObject *parent = nullptr);

    QString filePath() const;
    bool persist();                          // m_secrets verschlüsselt schreiben
    static void deriveKeys(const QString &password, const QByteArray &salt, int iter,
                           QByteArray &encKey, QByteArray &macKey);
    static QByteArray keystreamXor(const QByteArray &data, const QByteArray &encKey,
                                   const QByteArray &nonce);
    static QByteArray computeTag(const QByteArray &macKey, const QByteArray &salt,
                                 const QByteArray &nonce, const QByteArray &ct);
    static QByteArray randomBytes(int n);
    static bool constTimeEquals(const QByteArray &a, const QByteArray &b);

    bool m_unlocked = false;
    int m_iter = 210000;       // PBKDF2-Iterationen (aktuelle Vault-Parameter)
    QByteArray m_salt;         // KDF-Salt (pro Vault/Passwort)
    QByteArray m_encKey;       // abgeleitet, nur im entsperrten Zustand
    QByteArray m_macKey;       // abgeleitet, nur im entsperrten Zustand
    QMap<QString, QString> m_secrets;  // Klartext, nur im entsperrten Zustand
};

} // namespace qtmux

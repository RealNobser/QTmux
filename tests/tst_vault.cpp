#include <QtTest>
#include <QStandardPaths>
#include <QDir>
#include <QFile>

#include "SecretsVault.h"

using namespace qtmux;

// Tests für den verschlüsselten Secrets-Vault (QTMUX-22). QSettings-/QStandardPaths-
// Testmodus → eigene temporäre Pfade, keine echten Nutzerdaten.
class TestVault : public QObject {
    Q_OBJECT

    static QString vaultFile() {
        return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
               + QStringLiteral("/vault.json");
    }

private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("QTmux"));
        QCoreApplication::setApplicationName(QStringLiteral("QTmuxVaultTest"));
        QFile::remove(vaultFile());   // sauberer Start
    }

    void lifecycle() {
        auto *v = SecretsVault::instance();
        // Frisch: kein Vault, gesperrt.
        QVERIFY(!v->exists());
        QVERIFY(!v->isUnlocked());
        QVERIFY(!v->unlock(QStringLiteral("x")));   // ohne Vault nicht entsperrbar

        // Anlegen + entsperrt.
        QVERIFY(v->create(QStringLiteral("master-pw")));
        QVERIFY(v->exists());
        QVERIFY(v->isUnlocked());
        // Zweimal anlegen scheitert.
        QVERIFY(!v->create(QStringLiteral("other")));

        // Geheimnisse setzen/lesen.
        QVERIFY(v->setSecret(QStringLiteral("ssh/prod"), QStringLiteral("hunter2")));
        QVERIFY(v->setSecret(QStringLiteral("token"), QStringLiteral("abc123")));
        QCOMPARE(v->secret(QStringLiteral("ssh/prod")), QStringLiteral("hunter2"));
        QVERIFY(v->hasSecret(QStringLiteral("token")));
        QCOMPARE(v->secretNames().size(), 2);

        // Datei darf den Klartext NICHT enthalten (Verschlüsselung wirkt).
        QFile f(vaultFile());
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray raw = f.readAll();
        f.close();
        QVERIFY(!raw.contains("hunter2"));
        QVERIFY(!raw.contains("abc123"));

        // Sperren: Klartext weg.
        v->lock();
        QVERIFY(!v->isUnlocked());
        QVERIFY(v->secret(QStringLiteral("ssh/prod")).isEmpty());
        QVERIFY(v->secretNames().isEmpty());

        // Falsches Passwort scheitert, richtiges entschlüsselt (Persistenz-Roundtrip).
        QVERIFY(!v->unlock(QStringLiteral("falsch")));
        QVERIFY(!v->isUnlocked());
        QVERIFY(v->unlock(QStringLiteral("master-pw")));
        QCOMPARE(v->secret(QStringLiteral("ssh/prod")), QStringLiteral("hunter2"));
        QCOMPARE(v->secret(QStringLiteral("token")), QStringLiteral("abc123"));

        // Entfernen.
        QVERIFY(v->removeSecret(QStringLiteral("token")));
        QVERIFY(!v->hasSecret(QStringLiteral("token")));
        QCOMPARE(v->secretNames().size(), 1);

        // Master-Passwort ändern (altes falsch → scheitert; richtig → klappt).
        QVERIFY(!v->changeMasterPassword(QStringLiteral("falsch"), QStringLiteral("neu-pw")));
        QVERIFY(v->changeMasterPassword(QStringLiteral("master-pw"), QStringLiteral("neu-pw")));
        v->lock();
        QVERIFY(!v->unlock(QStringLiteral("master-pw")));   // altes PW gilt nicht mehr
        QVERIFY(v->unlock(QStringLiteral("neu-pw")));
        QCOMPARE(v->secret(QStringLiteral("ssh/prod")), QStringLiteral("hunter2"));
    }

    void tamperDetected() {
        // Manipulierte Ciphertext-Bytes → Tag-Prüfung schlägt fehl, unlock scheitert.
        auto *v = SecretsVault::instance();
        v->lock();
        QFile f(vaultFile());
        QVERIFY(f.open(QIODevice::ReadWrite));
        QByteArray data = f.readAll();
        const int idx = data.indexOf("\"ct\":");
        QVERIFY(idx > 0);
        // Ein Zeichen im base64-ct kippen (ein Stück hinter dem Key).
        data[idx + 8] = (data[idx + 8] == 'A') ? 'B' : 'A';
        f.seek(0); f.write(data); f.resize(data.size()); f.close();
        QVERIFY(!v->unlock(QStringLiteral("neu-pw")));
    }
};

QTEST_GUILESS_MAIN(TestVault)
#include "tst_vault.moc"

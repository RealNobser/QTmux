#include <QtTest>
#include <QSettings>
#include <QStandardPaths>

#include "ConnectionProfile.h"

using namespace qtmux;

/// Tests für die Connection-Profile-Registry (QTMUX-7). Läuft im QSettings-Testmodus
/// (eigene temporäre Settings-Datei), berührt also keine echten Nutzereinstellungen.
class TestProfiles : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("QTmux"));
        QCoreApplication::setApplicationName(QStringLiteral("QTmuxProfilesTest"));
    }

    void upsertPersistRemove() {
        auto *reg = ConnectionProfileRegistry::instance();
        // Sauberer Start (Testmodus kann zwischen Läufen Reste behalten).
        while (!reg->profiles().isEmpty())
            reg->removeProfile(reg->profiles().first().name);
        QCOMPARE(reg->profiles().size(), 0);

        // Anlegen (SSH).
        QVariantMap ssh;
        ssh[QStringLiteral("name")] = QStringLiteral("prod");
        ssh[QStringLiteral("type")] = 1;
        ssh[QStringLiteral("host")] = QStringLiteral("example.com");
        ssh[QStringLiteral("port")] = 2222;
        ssh[QStringLiteral("user")] = QStringLiteral("root");
        reg->saveProfile(ssh);
        QCOMPARE(reg->profiles().size(), 1);
        QCOMPARE(reg->profile(QStringLiteral("prod")).value(QStringLiteral("port")).toInt(), 2222);

        // Upsert: gleicher Name → ersetzen, nicht duplizieren.
        ssh[QStringLiteral("port")] = 22;
        reg->saveProfile(ssh);
        QCOMPARE(reg->profiles().size(), 1);
        QCOMPARE(reg->profile(QStringLiteral("prod")).value(QStringLiteral("port")).toInt(), 22);

        // Zweites Profil (Shell).
        QVariantMap sh;
        sh[QStringLiteral("name")] = QStringLiteral("lokal");
        sh[QStringLiteral("type")] = 0;
        sh[QStringLiteral("program")] = QStringLiteral("/bin/zsh");
        reg->saveProfile(sh);
        QCOMPARE(reg->profiles().size(), 2);

        // Namenloses Profil wird ignoriert.
        QVariantMap bad;
        bad[QStringLiteral("name")] = QStringLiteral("   ");
        bad[QStringLiteral("type")] = 0;
        reg->saveProfile(bad);
        QCOMPARE(reg->profiles().size(), 2);

        // Persistenz: eine frische QSettings sieht beide Profile.
        {
            QSettings s;
            const int n = s.beginReadArray(QStringLiteral("profiles"));
            QCOMPARE(n, 2);
            s.endArray();
        }

        // Entfernen.
        reg->removeProfile(QStringLiteral("prod"));
        QCOMPARE(reg->profiles().size(), 1);
        QCOMPARE(reg->profiles().first().name, QStringLiteral("lokal"));
    }
};

QTEST_GUILESS_MAIN(TestProfiles)
#include "tst_profiles.moc"

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
        ssh[QStringLiteral("loginScript")] = QStringLiteral("cd /srv\ntmux attach");
        ssh[QStringLiteral("passwordSecret")] = QStringLiteral("prod-ssh-pw");
        reg->saveProfile(ssh);
        QCOMPARE(reg->profiles().size(), 1);
        QCOMPARE(reg->profile(QStringLiteral("prod")).value(QStringLiteral("port")).toInt(), 2222);
        // Login-Script (QTMUX-23) wird mitgeführt/persistiert.
        QCOMPARE(reg->profile(QStringLiteral("prod")).value(QStringLiteral("loginScript")).toString(),
                 QStringLiteral("cd /srv\ntmux attach"));
        // Vault-Geheimnis-Name (QTMUX-22-Integration) wird mitgeführt/persistiert.
        QCOMPARE(reg->profile(QStringLiteral("prod")).value(QStringLiteral("passwordSecret")).toString(),
                 QStringLiteral("prod-ssh-pw"));

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

    // QTMUX-47: Die Verbindungen-Kategorie (qml/prefs/CatVerbindungen.qml) blendet den
    // SFTP-Knopf nur bei SSH-Profilen ein — Bedingung `modelData.type === 1`. Dieser
    // Guard sichert, dass der Typ-Wert über Persistenz/Auslesen erhalten bleibt (SSH=1,
    // Shell=0), sonst verschwände der SFTP-Knopf still oder erschiene fälschlich.
    void typePreservedForConnectionsUi() {
        auto *reg = ConnectionProfileRegistry::instance();
        while (!reg->profiles().isEmpty())
            reg->removeProfile(reg->profiles().first().name);

        QVariantMap ssh;
        ssh[QStringLiteral("name")] = QStringLiteral("edge");
        ssh[QStringLiteral("type")] = 1;   // SSH
        ssh[QStringLiteral("host")] = QStringLiteral("edge.example");
        reg->saveProfile(ssh);

        QVariantMap sh;
        sh[QStringLiteral("name")] = QStringLiteral("shell");
        sh[QStringLiteral("type")] = 0;    // lokale Shell
        reg->saveProfile(sh);

        QCOMPARE(reg->profile(QStringLiteral("edge")).value(QStringLiteral("type")).toInt(), 1);
        QCOMPARE(reg->profile(QStringLiteral("shell")).value(QStringLiteral("type")).toInt(), 0);
    }
};

QTEST_GUILESS_MAIN(TestProfiles)
#include "tst_profiles.moc"

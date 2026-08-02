// QTMUX-125: der vendierte Update-Kern (`appupdate`, third_party/updater/).
//
// Geprüft wird die Kette, die im Betrieb zählt — Signatur VOR dem Parsen, Manifest
// → Artefakt für DIESE Plattform, Download mit SHA-Abgleich, Start-Plan je Paketart
// — plus die reinen Entscheidungsfunktionen (Throttle/Skip/Downgrade).
//
// 🔑 Die Fixtures entstehen zur LAUFZEIT in einem QTemporaryDir und werden mit dem
// mitkompilierten Monocypher signiert. Committete Fixtures (MacPCANs Weg) brauchen
// `openssl` zum Regenerieren und einen Schlüssel, der nicht ins Repo darf — für eine
// vendierte Kopie ist das doppelte Pflege ohne Zusatznutzen.
// 🔑 Damit ein selbstsigniertes Fixture nicht bloß beweist, dass die Krypto zu sich
// selbst passt (eine kaputte, aber in sich konsistente Implementierung bestünde das),
// steht daneben ein **RFC-8032-Testvektor**: fremd erzeugte Bytes, die genau dann
// verifizieren, wenn hier echtes Ed25519/SHA-512 läuft — dieselbe Variante, die
// `openssl pkeyutl -sign -rawin` in der Publish-Kette erzeugt.

#include "update/UpdateChecker.hpp"
#include "update/UpdateManifest.hpp"
#include "update/UpdatePolicy.hpp"
#include "update/InstallerLauncher.hpp"
#include "update/Ed25519Verify.hpp"
#include "update/ed25519/monocypher-ed25519.h"

#include <QtTest>
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTimer>

using namespace appupdate;

namespace {

// Ein festes Schlüsselpaar aus einem festen Seed — reproduzierbar, kein Zufall.
struct TestKey {
    QByteArray secret;   // 64 Byte
    QByteArray pub;      // 32 Byte
};

TestKey makeKey() {
    QByteArray seed(32, '\x2a');
    TestKey k;
    k.secret.resize(64);
    k.pub.resize(32);
    crypto_ed25519_key_pair(reinterpret_cast<uint8_t *>(k.secret.data()),
                            reinterpret_cast<uint8_t *>(k.pub.data()),
                            reinterpret_cast<uint8_t *>(seed.data()));
    return k;
}

QByteArray sign(const TestKey &k, const QByteArray &message) {
    QByteArray sig(64, '\0');
    crypto_ed25519_sign(reinterpret_cast<uint8_t *>(sig.data()),
                        reinterpret_cast<const uint8_t *>(k.secret.constData()),
                        reinterpret_cast<const uint8_t *>(message.constData()),
                        static_cast<size_t>(message.size()));
    return sig;
}

bool writeFile(const QString &path, const QByteArray &bytes) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    return f.write(bytes) == bytes.size();
}

QString sha256Hex(const QByteArray &bytes) {
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

// Dreht den Event-Loop, bis `done` kippt oder die Wache zuschlägt.
void spinUntil(bool &done, int guardMs = 5000) {
    QEventLoop loop;
    QTimer guard;
    guard.setSingleShot(true);
    QObject::connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);
    guard.start(guardMs);
    while (!done && guard.isActive())
        loop.processEvents(QEventLoop::AllEvents, 20);
}

} // namespace

class TestUpdater : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tree;      // Fixture-Baum (spielt den Webspace)
    TestKey m_key;
    QByteArray m_payload;      // Inhalt des „Installers"

    // Legt <tree>/<product>/{manifest.json,manifest.json.sig} + Artefakt an.
    // `declaredSha` leer = korrekter Hash; `signOver` leer = über die echten Bytes.
    void buildProduct(const QString &product, const QString &version,
                      const QString &declaredSha = QString(),
                      const QByteArray &signOver = QByteArray()) {
        const QString root = m_tree.path() + QLatin1Char('/') + product;
        const QString artName = QStringLiteral("QTmux-%1.pkgtest").arg(version);
        QVERIFY(writeFile(root + QStringLiteral("/%1/%2").arg(version, artName), m_payload));

        QJsonObject art{
            {"url", QStringLiteral("%1/%2").arg(version, artName)},  // relativ!
            {"size", static_cast<qint64>(m_payload.size())},
            {"sha256", declaredSha.isEmpty() ? sha256Hex(m_payload) : declaredSha},
            {"kind", "dmg"},
        };
        // Alle drei Betriebssysteme belegen — QTmux liefert alle drei Installer.
        QJsonObject artifacts{
            {"win-x86_64", art}, {"macos-universal", art}, {"linux-x86_64", art}};

        const QJsonObject manifest{
            {"schema", 1},
            {"product", product},
            {"channel", "stable"},
            {"version", version},
            {"published", "2026-08-02"},
            {"notes", QJsonObject{{"de", "Deutsche Anmerkungen"},
                                  {"en", "English notes"}}},
            {"artifacts", artifacts},
        };
        const QByteArray bytes = QJsonDocument(manifest).toJson(QJsonDocument::Indented);
        QVERIFY(writeFile(root + QStringLiteral("/manifest.json"), bytes));
        QVERIFY(writeFile(root + QStringLiteral("/manifest.json.sig"),
                          sign(m_key, signOver.isEmpty() ? bytes : signOver)));
    }

    QUrl base() const { return QUrl::fromLocalFile(m_tree.path()); }

private slots:
    void initTestCase() {
        QVERIFY(m_tree.isValid());
        m_key = makeKey();
        m_payload = QByteArray("QTMUX-INSTALLER-FIXTURE").repeated(64);
        buildProduct(QStringLiteral("qtmux"), QStringLiteral("9.9.9"));
        buildProduct(QStringLiteral("sigfail"), QStringLiteral("9.9.9"), QString(),
                     QByteArray("nicht das Manifest"));
        buildProduct(QStringLiteral("shamismatch"), QStringLiteral("9.9.9"),
                     QString(64, QLatin1Char('a')));
    }

    // --- Manifest ----------------------------------------------------------
    void manifestParsesAndCarriesAllThreeArtifacts() {
        QFile f(m_tree.path() + QStringLiteral("/qtmux/manifest.json"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QString err;
        const auto m = UpdateManifest::fromJson(f.readAll(), &err);
        QVERIFY2(m.has_value(), qPrintable(err));
        QCOMPARE(m->product, QStringLiteral("qtmux"));
        QCOMPARE(m->version, QVersionNumber(9, 9, 9));
        // Der Schlüssel dieser Plattform MUSS belegt sein — QTmux hat DMG, MSI
        // und AppImage, ein „kein Paket für dieses OS" wäre hier ein Fehler.
        QVERIFY(m->artifacts.contains(UpdateManifest::currentArtifactKey()));
        QVERIFY(m->currentArtifact().has_value());
        QCOMPARE(m->artifacts.size(), 3);
    }

    void notesFallBackInsteadOfShowingNothing() {
        QJsonObject j{{"schema", 1}, {"product", "qtmux"}, {"version", "1.0.0"},
                      {"notes", QJsonObject{{"de", "Nur Deutsch"}}},
                      {"artifacts", QJsonObject{}}};
        const auto m = UpdateManifest::fromJson(QJsonDocument(j).toJson());
        QVERIFY(m.has_value());
        QCOMPARE(m->notes(QStringLiteral("de")), QStringLiteral("Nur Deutsch"));
        // Kein englischer Text vorhanden → der deutsche, statt einer leeren Seite.
        QCOMPARE(m->notes(QStringLiteral("en")), QStringLiteral("Nur Deutsch"));
    }

    void brokenManifestIsRejectedWithAReason() {
        QString err;
        QVERIFY(!UpdateManifest::fromJson(QByteArray("{kein json"), &err).has_value());
        QVERIFY(!err.isEmpty());
    }

    // --- Entscheidungsregeln ------------------------------------------------
    void policyDecidesNewerSkipAndDowngrade() {
        const QVersionNumber cur(1, 7, 1);
        QVERIFY(policy::isNewer(QVersionNumber(1, 8, 0), cur));
        QVERIFY(!policy::isNewer(cur, cur));                      // gleich ist kein Update
        QVERIFY(policy::isDowngrade(QVersionNumber(1, 7, 0), cur));
        QVERIFY(policy::isSkipped(QVersionNumber(1, 8, 0), QStringLiteral("1.8.0")));
        // Eine SPÄTERE Version hebt das Überspringen implizit auf.
        QVERIFY(!policy::isSkipped(QVersionNumber(1, 9, 0), QStringLiteral("1.8.0")));
        QVERIFY(!policy::isSkipped(QVersionNumber(1, 8, 0), QString()));
    }

    void autoCheckRunsOncePerDay() {
        const QDateTime now = QDateTime::currentDateTime();
        QVERIFY(policy::autoCheckDue(QString(), now));                   // nie geprüft
        QVERIFY(policy::autoCheckDue(QStringLiteral("kaputt"), now));    // unlesbar
        QVERIFY(!policy::autoCheckDue(now.toString(Qt::ISODate), now));  // heute schon
        QVERIFY(policy::autoCheckDue(now.addDays(-1).toString(Qt::ISODate), now));
    }

    // --- Signatur -----------------------------------------------------------
    // Fremde Bytes: RFC 8032 §7.1, TEST 2 (Nachricht = 0x72). Belegt echtes
    // Ed25519 mit SHA-512 — die Variante, die openssl in der Publish-Kette
    // erzeugt. Monocyphers ähnlich benanntes `crypto_eddsa_check` (BLAKE2b)
    // würde hier scheitern; genau davor warnt der Kopf von Ed25519Verify.hpp.
    void rfc8032VectorVerifies() {
        const QByteArray pub = QByteArray::fromHex(
            "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c");
        const QByteArray sig = QByteArray::fromHex(
            "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da"
            "085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00");
        const QByteArray msg = QByteArray::fromHex("72");
        QVERIFY(verifyEd25519(sig, pub, msg));
        // Gegenprobe: ein einziges verändertes Nachrichtenbyte darf nicht durchgehen.
        QVERIFY(!verifyEd25519(sig, pub, QByteArray::fromHex("73")));
        // Falsche Längen werden abgewiesen, nicht behauptet (Netzwerk-Bytes!).
        QVERIFY(!verifyEd25519(sig.left(63), pub, msg));
        QVERIFY(!verifyEd25519(sig, pub.left(31), msg));
    }

    // --- Checker gegen file://-Fixtures --------------------------------------
    void checkerVerifiesParsesAndDownloads() {
        UpdateChecker checker(base(), QStringLiteral("qtmux"), m_key.pub);
        bool done = false;
        std::optional<UpdateManifest> manifest;
        QString error;
        checker.checkForUpdate([&](std::optional<UpdateManifest> m, QString e) {
            manifest = std::move(m); error = std::move(e); done = true;
        });
        spinUntil(done);
        QVERIFY2(manifest.has_value(), qPrintable(error));
        QCOMPARE(manifest->version, QVersionNumber(9, 9, 9));

        const auto art = manifest->currentArtifact();
        QVERIFY(art.has_value());

        QTemporaryDir dest;
        QVERIFY(dest.isValid());
        bool dl = false;
        QString localPath, dlError;
        checker.downloadArtifact(*art, dest.path(), [&](QString p, QString e) {
            localPath = std::move(p); dlError = std::move(e); dl = true;
        });
        spinUntil(dl);
        QVERIFY2(dlError.isEmpty(), qPrintable(dlError));
        QVERIFY(!localPath.isEmpty());
        QFile got(localPath);
        QVERIFY(got.open(QIODevice::ReadOnly));
        QCOMPARE(got.readAll(), m_payload);
    }

    // Signatur über ANDERE Bytes: das Manifest ist syntaktisch einwandfrei und
    // darf den Parser trotzdem nie erreichen.
    void badSignatureNeverReachesTheParser() {
        UpdateChecker checker(base(), QStringLiteral("sigfail"), m_key.pub);
        bool done = false;
        std::optional<UpdateManifest> manifest;
        QString error;
        checker.checkForUpdate([&](std::optional<UpdateManifest> m, QString e) {
            manifest = std::move(m); error = std::move(e); done = true;
        });
        spinUntil(done);
        QVERIFY(!manifest.has_value());
        QVERIFY(error.contains(QStringLiteral("signature"), Qt::CaseInsensitive));
    }

    // Ein FREMDER Schlüssel darf ein korrekt signiertes Manifest nicht annehmen —
    // sonst prüfte der Test oben nur, dass irgendeine Prüfung stattfindet.
    void foreignKeyRejectsAValidManifest() {
        QByteArray otherSeed(32, '\x7f');
        QByteArray otherSecret(64, '\0'), otherPub(32, '\0');
        crypto_ed25519_key_pair(reinterpret_cast<uint8_t *>(otherSecret.data()),
                                reinterpret_cast<uint8_t *>(otherPub.data()),
                                reinterpret_cast<uint8_t *>(otherSeed.data()));
        UpdateChecker checker(base(), QStringLiteral("qtmux"), otherPub);
        bool done = false;
        std::optional<UpdateManifest> manifest;
        QString error;
        checker.checkForUpdate([&](std::optional<UpdateManifest> m, QString e) {
            manifest = std::move(m); error = std::move(e); done = true;
        });
        spinUntil(done);
        QVERIFY(!manifest.has_value());
        QVERIFY(!error.isEmpty());
    }

    // Falscher SHA im Manifest: der Download wird GELÖSCHT. Ein beschädigter
    // Installer darf nicht liegen bleiben, um später doppelgeklickt zu werden.
    void shaMismatchDeletesTheDownload() {
        UpdateChecker checker(base(), QStringLiteral("shamismatch"), m_key.pub);
        bool done = false;
        std::optional<UpdateManifest> manifest;
        checker.checkForUpdate([&](std::optional<UpdateManifest> m, QString) {
            manifest = std::move(m); done = true;
        });
        spinUntil(done);
        QVERIFY(manifest.has_value());
        const auto art = manifest->currentArtifact();
        QVERIFY(art.has_value());

        QTemporaryDir dest;
        QVERIFY(dest.isValid());
        bool dl = false;
        QString localPath, dlError;
        checker.downloadArtifact(*art, dest.path(), [&](QString p, QString e) {
            localPath = std::move(p); dlError = std::move(e); dl = true;
        });
        spinUntil(dl);
        QVERIFY(localPath.isEmpty());
        QVERIFY(dlError.contains(QStringLiteral("sha256")));
        QCOMPARE(QDir(dest.path()).entryList(QDir::Files).size(), 0);
    }

    // --- Installer-Start ----------------------------------------------------
    // Nur der PLAN wird geprüft (die reine Hälfte) — ausführen würde hier einen
    // Installer starten.
    void launchPlanPerKind() {
        const QString path = QStringLiteral("/tmp/QTmux-9.9.9");
        const auto msi = installerLaunchPlan(path + QStringLiteral(".msi"),
                                             QStringLiteral("msi"), QString());
        QVERIFY(msi.has_value());
        QCOMPARE(msi->program, QStringLiteral("msiexec"));
        QVERIFY(msi->arguments.contains(QStringLiteral("/i")));

        const auto dmg = installerLaunchPlan(path + QStringLiteral(".dmg"),
                                             QStringLiteral("dmg"), QString());
        QVERIFY(dmg.has_value());
        QCOMPARE(dmg->program, QStringLiteral("open"));

        // AppImage OHNE laufendes $APPIMAGE: es gibt nichts zu ersetzen.
        QVERIFY(!installerLaunchPlan(path + QStringLiteral(".AppImage"),
                                     QStringLiteral("appimage"), QString()).has_value());
        const auto ai = installerLaunchPlan(path + QStringLiteral(".AppImage"),
                                            QStringLiteral("appimage"),
                                            QStringLiteral("/opt/QTmux.AppImage"));
        QVERIFY(ai.has_value());
        QVERIFY(ai->selfReplace);

        // Unbekannte Paketart: kein Plan statt eines geratenen Kommandos.
        QVERIFY(!installerLaunchPlan(path, QStringLiteral("exe"), QString()).has_value());
    }
};

QTEST_MAIN(TestUpdater)
#include "tst_updater.moc"

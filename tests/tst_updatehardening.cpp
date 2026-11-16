// Update-Härtung, Vertrag V1 (2026-11-16): die QTmux-Hälfte über dem vendierten
// Hub-Kern. Der Kern (third_party/updater/, Hub-Commit im UPSTREAM.md-Pin) prüft
// Produkt, Kanal und Seriennummer und begrenzt den Download auf `size`. Er ist im
// Hub mit eigenen Tests belegt. Hier geht es um das, was NUR QTmux leistet:
//
//  • `UpdateViewModel` gibt dem Kern einen Speicher für `lastSerial`
//    (`update/last_serial/qtmux` in den QSettings) und den Kanal `stable`.
//    Ohne die Verdrahtung gäbe es keinen Replay-Schutz, und der Kern meldete das
//    nur per qWarning — darum ist jede Warnung „without a serial store“ hier ein
//    roter Test (init()).
//  • Die manuelle Prüfung nennt jede Ablehnung mit eigener Überschrift und
//    eigenem Satz, die stille Prüfung bleibt still und sieht nie wie „aktuell“ aus.
//  • Ein Download mit falscher Datenmenge scheitert mit eigenem Satz, ohne Rest.
//
// 🔑 Die Bäume entstehen zur LAUFZEIT und werden mit einem festen Wegwerf-Schlüssel
// signiert (Monocypher, wie tst_updater); `setUpdatePublicKey` reicht ihn an den
// Checker. So braucht kein Fall den Produktionsschlüssel. Dass die AUSGELIEFERTE
// App echte Signaturen annimmt, belegt weiter tst_updateviewmodel.
//
// Testnamen nennen die Regel des Vertrags (A1–A3) und, wo es passt, die ID der
// Spezifikation (T-4 fremdes Produkt, T-20 Größengrenze).

#include <QtTest>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>

#include "UpdateViewModel.h"
#include "update/UpdatePolicy.hpp"
#include "update/ed25519/monocypher-ed25519.h"

using namespace qtmux;

namespace {

constexpr auto kSerialKey = "update/last_serial/qtmux";

// Ein festes Schlüsselpaar aus einem festen Seed — reproduzierbar, kein Zufall,
// und erkennbar KEIN Produktionsschlüssel.
struct TestKey {
    QByteArray secret;   // 64 Byte
    QByteArray pub;      // 32 Byte
};

TestKey makeKey() {
    QByteArray seed(32, '\x51');
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

// Was ein Probebaum trägt. Vorgabe = ein gültiges Angebot 9.9.9 für QTmux auf
// `stable`, ohne `serial` (wie jedes Manifest vor Vertrag V1).
struct Spec {
    QString product = QStringLiteral("qtmux");
    std::optional<QString> channel = QStringLiteral("stable");   // nullopt = fehlt
    QString version = QStringLiteral("9.9.9");
    std::optional<qint64> serial;                                 // nullopt = fehlt
    QByteArray payload = QByteArrayLiteral("QTmux-Paket fuer den Test\n");
    // Abweichung der Datei auf dem Server von `size` im Manifest (+1/−1).
    int sizeDelta = 0;
    bool forgeSignature = false;   // Signatur über ANDERE Bytes
};

// Legt <root>/qtmux/manifest.json(.sig) und die drei Artefakte an. `size` und
// `sha256` beschreiben immer `payload`; die Datei auf dem Server weicht davon
// um `sizeDelta` Bytes ab (angehängt bzw. abgeschnitten).
bool makeTree(const QString &root, const TestKey &key, const Spec &spec) {
    const QByteArray hash =
        QCryptographicHash::hash(spec.payload, QCryptographicHash::Sha256).toHex();
    QByteArray served = spec.payload;
    if (spec.sizeDelta > 0) served.append(QByteArray(spec.sizeDelta, 'X'));
    if (spec.sizeDelta < 0) served.chop(-spec.sizeDelta);

    QJsonObject artifacts;
    const struct { const char *key; const char *file; const char *kind; } arts[] = {
        { "macos-universal", "QTmux-%1-macos.dmg", "dmg" },
        { "win-x86_64", "QTmux-%1-win64.msi", "msi" },
        { "linux-x86_64", "QTmux-%1-x86_64.AppImage", "appimage" },
    };
    for (const auto &a : arts) {
        const QString file = QString::fromLatin1(a.file).arg(spec.version);
        const QString rel = spec.version + QLatin1Char('/') + file;
        if (!writeFile(root + QStringLiteral("/qtmux/") + rel, served)) return false;
        artifacts.insert(QLatin1String(a.key),
                         QJsonObject{ { QStringLiteral("url"), rel },
                                      { QStringLiteral("size"), qint64(spec.payload.size()) },
                                      { QStringLiteral("sha256"), QString::fromLatin1(hash) },
                                      { QStringLiteral("kind"), QLatin1String(a.kind) } });
    }

    QJsonObject m{ { QStringLiteral("schema"), 1 },
                   { QStringLiteral("product"), spec.product },
                   { QStringLiteral("version"), spec.version },
                   { QStringLiteral("published"), QStringLiteral("2026-11-16") },
                   { QStringLiteral("notes"),
                     QJsonObject{ { QStringLiteral("de"), QStringLiteral("Probe") },
                                  { QStringLiteral("en"), QStringLiteral("Probe") } } },
                   { QStringLiteral("artifacts"), artifacts } };
    if (spec.channel) m.insert(QStringLiteral("channel"), *spec.channel);
    if (spec.serial) m.insert(QStringLiteral("serial"), *spec.serial);

    const QByteArray bytes = QJsonDocument(m).toJson();
    const QByteArray sig =
        sign(key, spec.forgeSignature ? QByteArrayLiteral("nicht das Manifest") : bytes);
    return writeFile(root + QStringLiteral("/qtmux/manifest.json"), bytes)
           && writeFile(root + QStringLiteral("/qtmux/manifest.json.sig"), sig);
}

void spinUntil(bool &done, int guardMs = 5000) {
    QEventLoop loop;
    QTimer guard;
    guard.setSingleShot(true);
    QObject::connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);
    guard.start(guardMs);
    while (!done && guard.isActive())
        loop.processEvents(QEventLoop::AllEvents, 20);
}

// Winziger HTTP/1.0-Server über einem Probebaum: der echte Transportweg, auf dem
// ein Server mehr oder weniger liefert, als das Manifest sagt. Content-Length
// beschreibt die Datei, wie sie auf dem Server liegt — also ehrlich falsch.
class TreeHttpServer : public QTcpServer {
public:
    explicit TreeHttpServer(QString root) : m_root(std::move(root)) {}
    QString base() const { return QStringLiteral("http://127.0.0.1:%1").arg(serverPort()); }

protected:
    void incomingConnection(qintptr handle) override {
        auto *sock = new QTcpSocket(this);
        sock->setSocketDescriptor(handle);
        connect(sock, &QTcpSocket::readyRead, this, [this, sock] {
            const QList<QByteArray> parts = sock->readAll().split(' ');
            if (parts.size() < 2) { sock->disconnectFromHost(); return; }
            QString path = QString::fromUtf8(parts.at(1));
            const int q = path.indexOf(QLatin1Char('?'));
            if (q >= 0) path.truncate(q);
            QFile f(m_root + QUrl::fromPercentEncoding(path.toUtf8()));
            if (!f.open(QIODevice::ReadOnly)) {
                sock->write("HTTP/1.0 404 Not Found\r\nContent-Length: 0\r\n\r\n");
            } else {
                const QByteArray body = f.readAll();
                sock->write("HTTP/1.0 200 OK\r\nContent-Type: application/octet-stream\r\n"
                            "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n");
                sock->write(body);
            }
            sock->flush();
            sock->disconnectFromHost();
        });
        connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
    }

private:
    QString m_root;
};

} // namespace

class tst_updatehardening : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_settingsDir;
    TestKey m_key = makeKey();

    // Ein ViewModel auf einem frischen Probebaum. Der Baum lebt so lange wie `dir`.
    void point(UpdateViewModel &vm, QTemporaryDir &dir, const Spec &spec) {
        QVERIFY(dir.isValid());
        QVERIFY(makeTree(dir.path(), m_key, spec));
        vm.setUpdatePublicKey(m_key.pub);
        vm.setBaseUrl(QUrl::fromLocalFile(dir.path()).toString());
    }

    static void clearThrottle() {
        QSettings s;
        s.remove(QStringLiteral("update/lastCheck"));
        s.sync();   // Begründung in tst_updateviewmodel::clearThrottle
    }

    static void setLastSerial(const QVariant &v) {
        QSettings s;
        s.setValue(QLatin1String(kSerialKey), v);
        s.sync();
    }

    static QVariant lastSerialValue() { return QSettings().value(QLatin1String(kSerialKey)); }

    bool runCheck(UpdateViewModel &vm, bool manual) {
        if (!manual) clearThrottle();
        bool done = false;
        auto c = connect(&vm, &UpdateViewModel::checkFinished, [&done](bool) { done = true; });
        if (manual) vm.checkNow(); else vm.checkOnStartup();
        spinUntil(done);
        disconnect(c);
        return done;
    }

    bool runDownload(UpdateViewModel &vm) {
        bool done = false;
        auto c = connect(&vm, &UpdateViewModel::stateChanged, [&vm, &done] {
            done = vm.state() == UpdateViewModel::Ready || vm.state() == UpdateViewModel::Failed;
        });
        vm.download();
        spinUntil(done);
        disconnect(c);
        return done;
    }

    // Der Dateiname, den der Kern für DIESE Plattform lädt (wie in makeTree).
    static QString thisPlatformsFile(const QString &version) {
#if defined(Q_OS_MACOS)
        return QStringLiteral("QTmux-%1-macos.dmg").arg(version);
#elif defined(Q_OS_WIN)
        return QStringLiteral("QTmux-%1-win64.msi").arg(version);
#else
        return QStringLiteral("QTmux-%1-x86_64.AppImage").arg(version);
#endif
    }

    // Was im Update-Ordner liegt (Dateien samt versteckter).
    static QStringList leftovers(const UpdateViewModel &vm) {
        return QDir(vm.downloadDir()).entryList(QDir::Files | QDir::Hidden);
    }

    // Die manuelle Prüfung lehnt ab und sagt es: Zustand Failed, eigene
    // Überschrift, eigener Satz, der Grund des Kerns darunter — und nichts im Angebot.
    void expectRefusedLoudly(UpdateViewModel &vm, const QString &title,
                             const QString &sentencePart, const QString &hubReason) {
        QCOMPARE(vm.state(), int(UpdateViewModel::Failed));
        QVERIFY(!vm.updateAvailable());
        QCOMPARE(vm.remoteVersion(), QString());
        QCOMPARE(vm.failureTitle(), title);
        QVERIFY2(vm.lastError().contains(sentencePart), qPrintable(vm.lastError()));
        QVERIFY2(vm.lastError().contains(hubReason), qPrintable(vm.lastError()));
    }

    // Die stille Prüfung lehnt dasselbe ab, ohne etwas zu zeigen — und ohne
    // „aktuell“ zu behaupten (UpToDate). Kein updateFound, kein Fehlertext.
    void expectRefusedSilently(UpdateViewModel &vm) {
        QSignalSpy found(&vm, &UpdateViewModel::updateFound);
        QVERIFY(runCheck(vm, /*manual=*/false));
        QCOMPARE(vm.state(), int(UpdateViewModel::Idle));
        QCOMPARE(found.count(), 0);
        QCOMPARE(vm.lastError(), QString());
        QCOMPARE(vm.failureTitle(), QString());
    }

    static QString titleReplay() { return QStringLiteral("Veraltete Update-Information abgelehnt"); }
    static QString titleWrong() { return QStringLiteral("Unpassende Update-Information abgelehnt"); }
    static QString sentenceReplay() { return QStringLiteral("älter ist als die, die diese Installation schon kennt"); }
    static QString sentenceWrong() { return QStringLiteral("nicht für QTmux auf diesem Kanal bestimmt"); }
    static QString sentenceSize() { return QStringLiteral("andere Datenmenge geschickt"); }

private slots:
    void initTestCase() {
        QVERIFY(m_settingsDir.isValid());
        QStandardPaths::setTestModeEnabled(true);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
        QCoreApplication::setOrganizationName(QStringLiteral("QTmuxTest"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_updatehardening"));
        QCOMPARE(m_key.pub.size(), 32);
    }

    // 🔑 Ein Checker ohne Speicher prüft das Produkt, schützt aber nicht vor
    // Replay — und sagt das nur per qWarning. In JEDEM Fall hier ist das rot.
    void init() {
        QTest::failOnWarning(QRegularExpression(QStringLiteral("without a serial store")));
    }

    void cleanup() {
        QSettings s;
        s.clear();
        s.sync();
        UpdateViewModel probe;
        QDir(probe.downloadDir()).removeRecursively();
    }

    // ---- Verdrahtung ----------------------------------------------------------

    void theKeyAndChannelAreTheFamilyContract() {
        // Hub-Schreibweise, nicht eine eigene: alle Apps schreiben gleich.
        QCOMPARE(appupdate::policy::lastSerialSettingsKey(QStringLiteral("qtmux")),
                 QLatin1String(kSerialKey));
        QCOMPARE(UpdateViewModel::expectedChannel(), QStringLiteral("stable"));
    }

    // ---- A2.1 Produktbindung ----------------------------------------------------

    void A2_1_T4_aManifestOfAnotherProductIsRefusedDespiteAValidSignature() {
        QTemporaryDir dir;
        UpdateViewModel vm;
        Spec spec;
        spec.product = QStringLiteral("raftng");
        spec.serial = 1794856570;
        point(vm, dir, spec);
        // Der Kern sucht unter /qtmux/ — der Baum liegt dort, trägt aber `raftng`.
        QVERIFY(runCheck(vm, true));
        expectRefusedLoudly(vm, titleWrong(), sentenceWrong(), QStringLiteral("'raftng'"));
        // Gespeichert wird erst nach bestandener Produktprüfung, nie vorher.
        QVERIFY(!lastSerialValue().isValid());
        expectRefusedSilently(vm);
    }

    void A2_1_aWrongChannelIsRefused() {
        QTemporaryDir dir;
        UpdateViewModel vm;
        Spec spec;
        spec.channel = QStringLiteral("beta");
        point(vm, dir, spec);
        QVERIFY(runCheck(vm, true));
        expectRefusedLoudly(vm, titleWrong(), sentenceWrong(), QStringLiteral("channel 'beta'"));
        expectRefusedSilently(vm);
    }

    void A2_1_aMissingChannelIsRefusedNotTakenAsAnyChannel() {
        QTemporaryDir dir;
        UpdateViewModel vm;
        Spec spec;
        spec.channel.reset();
        point(vm, dir, spec);
        QVERIFY(runCheck(vm, true));
        expectRefusedLoudly(vm, titleWrong(), sentenceWrong(),
                            QStringLiteral("names no channel"));
        expectRefusedSilently(vm);
    }

    // ---- A2.2 Seriennummer ------------------------------------------------------

    void A2_2_withoutAnySerialYetAManifestWithoutSerialIsAccepted() {
        // Der Übergang: lastSerial = 0 nimmt auch ohne `serial` an — so sehen
        // heute alle Live-Manifeste aus. Und es wird nichts gespeichert.
        QTemporaryDir dir;
        UpdateViewModel vm;
        point(vm, dir, Spec{});
        QVERIFY(runCheck(vm, true));
        QCOMPARE(vm.state(), int(UpdateViewModel::Available));
        QCOMPARE(vm.failureTitle(), QString());
        QVERIFY(!lastSerialValue().isValid());
    }

    void A2_2_anAcceptedSerialIsStoredEvenWithoutAnInstall() {
        QTemporaryDir dir;
        UpdateViewModel vm;
        Spec spec;
        spec.serial = 1794856570;
        point(vm, dir, spec);
        QSignalSpy found(&vm, &UpdateViewModel::updateFound);
        QVERIFY(runCheck(vm, /*manual=*/false));   // auch der stille Check speichert
        QCOMPARE(found.count(), 1);
        QCOMPARE(vm.state(), int(UpdateViewModel::Available));
        // Als ZAHL, nicht als Text — und gleich auf der Platte (sync im Speicher).
        QSettings onDisk;
        QCOMPARE(onDisk.value(QLatin1String(kSerialKey)).toULongLong(), 1794856570ull);
        QCOMPARE(UpdateViewModel::storedLastSerial(QStringLiteral("qtmux")), 1794856570u);
        // Dasselbe Manifest noch einmal: kein Replay (≥ zählt).
        QVERIFY(runCheck(vm, true));
        QCOMPARE(vm.state(), int(UpdateViewModel::Available));
    }

    void A2_2_anOlderSerialIsAReplay() {
        setLastSerial(qulonglong(2000));
        QTemporaryDir dir;
        UpdateViewModel vm;
        Spec spec;
        spec.serial = 1999;
        point(vm, dir, spec);
        QVERIFY(runCheck(vm, true));
        expectRefusedLoudly(vm, titleReplay(), sentenceReplay(), QStringLiteral("1999"));
        QCOMPARE(lastSerialValue().toULongLong(), 2000ull);   // nie rückwärts
        expectRefusedSilently(vm);
    }

    void A2_2_aMissingSerialAfterOneWasAcceptedIsAReplay() {
        setLastSerial(qulonglong(2000));
        QTemporaryDir dir;
        UpdateViewModel vm;
        point(vm, dir, Spec{});
        QVERIFY(runCheck(vm, true));
        expectRefusedLoudly(vm, titleReplay(), sentenceReplay(), QStringLiteral("serial"));
        QCOMPARE(lastSerialValue().toULongLong(), 2000ull);
        expectRefusedSilently(vm);
    }

    void A2_2_aRollbackRepublishedWithANewSerialIsAnOfferNotAReplay() {
        // A1: Ein Rückzug veröffentlicht die alte Version mit NEUEM serial.
        setLastSerial(qulonglong(2000));
        QTemporaryDir dir;
        UpdateViewModel vm;
        Spec spec;
        spec.version = QStringLiteral("0.0.1");
        spec.serial = 3000;
        point(vm, dir, spec);
        QVERIFY(runCheck(vm, true));
        QCOMPARE(vm.state(), int(UpdateViewModel::Available));
        QVERIFY(vm.isDowngrade());
        QCOMPARE(lastSerialValue().toULongLong(), 3000ull);
    }

    void A2_2_aForgedSignatureStoresNothing() {
        QTemporaryDir dir;
        UpdateViewModel vm;
        Spec spec;
        spec.serial = 4000000000;
        spec.forgeSignature = true;
        point(vm, dir, spec);
        QVERIFY(runCheck(vm, true));
        QCOMPARE(vm.state(), int(UpdateViewModel::Failed));
        QCOMPARE(vm.failureTitle(), QString());   // die allgemeine Überschrift
        QVERIFY(!lastSerialValue().isValid());
    }

    void A2_2_anUnreadableStoredValueCountsAsZero_data() {
        QTest::addColumn<QVariant>("stored");
        QTest::newRow("Text") << QVariant(QStringLiteral("abc"));
        // 2^32 + 4000000000: abgeschnitten auf 32 Bit bliebe 4000000000 übrig —
        // größer als das Angebot unten, ein Fehler im Bereichsriegel würde also
        // als Replay sichtbar. (2^32 allein schnitte zufällig auf 0 und sähe
        // auch ohne Riegel richtig aus.)
        QTest::newRow("über uint32") << QVariant(qulonglong(8294967296ull));
        QTest::newRow("negativ") << QVariant(qlonglong(-5));
    }

    void A2_2_anUnreadableStoredValueCountsAsZero() {
        // Ein verbogener Wert sperrt nie alle künftigen Updates aus — er zählt
        // als 0, und das nächste angenommene `serial` ersetzt ihn.
        QFETCH(QVariant, stored);
        setLastSerial(stored);
        QCOMPARE(UpdateViewModel::storedLastSerial(QStringLiteral("qtmux")), 0u);
        QTemporaryDir dir;
        UpdateViewModel vm;
        Spec spec;
        spec.serial = 1794856570;
        point(vm, dir, spec);
        QVERIFY(runCheck(vm, true));
        QCOMPARE(vm.state(), int(UpdateViewModel::Available));
        QCOMPARE(lastSerialValue().toULongLong(), 1794856570ull);
    }

    void A2_2_theSerialIsKeptPerProduct() {
        // Ein Wert eines anderen Produkts zählt für QTmux nicht.
        QSettings s;
        s.setValue(appupdate::policy::lastSerialSettingsKey(QStringLiteral("raftng")),
                   qulonglong(4000000000ull));
        s.sync();
        QTemporaryDir dir;
        UpdateViewModel vm;
        Spec spec;
        spec.serial = 1000;
        point(vm, dir, spec);
        QVERIFY(runCheck(vm, true));
        QCOMPARE(vm.state(), int(UpdateViewModel::Available));
        QCOMPARE(lastSerialValue().toULongLong(), 1000ull);
    }

    // ---- A3 Größengrenze ----------------------------------------------------------

    void A3_T20_aDownloadWithTheWrongAmountFailsWithItsOwnSentence_data() {
        QTest::addColumn<int>("delta");
        QTest::addColumn<bool>("http");
        QTest::newRow("size+1 file") << 1 << false;
        QTest::newRow("size-1 file") << -1 << false;
        QTest::newRow("size+1 http") << 1 << true;
        QTest::newRow("size-1 http") << -1 << true;
    }

    void A3_T20_aDownloadWithTheWrongAmountFailsWithItsOwnSentence() {
        QFETCH(int, delta);
        QFETCH(bool, http);
        QTemporaryDir dir;
        UpdateViewModel vm;
        Spec spec;
        spec.sizeDelta = delta;
        point(vm, dir, spec);
        TreeHttpServer srv(dir.path());
        if (http) {
            QVERIFY(srv.listen(QHostAddress::LocalHost));
            vm.setBaseUrl(srv.base());
        }

        // Eine ältere Datei gleichen Namens bleibt unberührt (A3).
        const QString old = vm.downloadDir() + QLatin1Char('/') + thisPlatformsFile(spec.version);
        QVERIFY(writeFile(old, QByteArrayLiteral("aeltere Datei")));

        QVERIFY(runCheck(vm, true));
        QCOMPARE(vm.state(), int(UpdateViewModel::Available));
        QVERIFY(runDownload(vm));
        QCOMPARE(vm.state(), int(UpdateViewModel::Failed));
        QVERIFY2(vm.lastError().contains(sentenceSize()), qPrintable(vm.lastError()));
        QCOMPARE(vm.failureTitle(), QString());   // „Update fehlgeschlagen“
        QCOMPARE(vm.downloadedPath(), QString());

        QFile f(old);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), QByteArrayLiteral("aeltere Datei"));
        f.close();
        QVERIFY(QFile::remove(old));
        // Und kein Rest: weder die Teildatei noch eine `.part`-Zwischendatei.
        QVERIFY2(leftovers(vm).isEmpty(), qPrintable(leftovers(vm).join(u',')));
    }

    void A3_theExactSizeStillDownloads() {
        // Positivkontrolle: Die Grenze trifft nur die falsche Menge.
        QTemporaryDir dir;
        UpdateViewModel vm;
        point(vm, dir, Spec{});
        QVERIFY(runCheck(vm, true));
        QVERIFY(runDownload(vm));
        QCOMPARE(vm.state(), int(UpdateViewModel::Ready));
        QVERIFY(QFile::exists(vm.downloadedPath()));
    }
};

QTEST_GUILESS_MAIN(tst_updatehardening)
#include "tst_updatehardening.moc"

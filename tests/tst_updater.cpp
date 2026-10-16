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

#if !defined(Q_OS_WIN)
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

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

// Reste der Download-Zwischendatei <Name>.XXXXXX.part — nach JEDEM Ausgang
// (Erfolg, Hash-Fehler, abgelehntes Ersetzen) muss die Liste leer sein.
QStringList partFiles(const QString &dir) {
    return QDir(dir).entryList({QStringLiteral("*.part")},
                               QDir::Files | QDir::Hidden | QDir::System);
}

QByteArray contentOf(const QString &path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

// Datiert die letzte Schreibzeit einer Datei um `seconds` zurück — so alt, wie ein
// Rest nach einem harten Ende der App beim nächsten Versuch wäre.
bool age(const QString &path, int seconds) {
    QFile f(path);
    if (!f.open(QIODevice::ReadWrite)) return false;
    return f.setFileTime(QDateTime::currentDateTime().addSecs(-seconds),
                         QFileDevice::FileModificationTime);
}

// Alle Einträge eines Ordners (Dateien, Ordner, Links), sortiert.
QStringList entriesOf(const QString &dir) {
    QStringList e = QDir(dir).entryList(QDir::AllEntries | QDir::Hidden | QDir::System
                                        | QDir::NoDotAndDotDot);
    e.sort();
    return e;
}

// Fängt die Warnungen des Update-Kerns mit (er meldet jedes Aufräumen per
// qWarning) und reicht alles an den Handler von QtTest weiter.
class WarningLog {
public:
    WarningLog() { s_self = this; s_prev = qInstallMessageHandler(&WarningLog::handle); }
    ~WarningLog() { qInstallMessageHandler(s_prev); s_self = nullptr; }
    QString text() const { return m_lines.join(QLatin1Char('\n')); }

private:
    static void handle(QtMsgType t, const QMessageLogContext &c, const QString &m) {
        if (s_self && t == QtWarningMsg) s_self->m_lines << m;
        if (s_prev) s_prev(t, c, m);
    }
    static inline WarningLog *s_self = nullptr;
    static inline QtMessageHandler s_prev = nullptr;
    QStringList m_lines;
};

#if !defined(Q_OS_WIN)
// Inode des NAMENS selbst (lstat: ein Symlink zählt als er selbst). 0 = fehlt.
quint64 inodeOf(const QString &path) {
    struct stat st;
    return ::lstat(QFile::encodeName(path).constData(), &st) == 0 ? quint64(st.st_ino) : 0;
}
#endif

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

    // Prüft das Manifest von `product` und lädt dessen Artefakt nach `destDir` —
    // der ganze Weg bis zum fertigen Ersetzen des Zielnamens. `finished == false`
    // heißt: die Wache (je Schritt 5 s) ist abgelaufen, der Weg hängt.
    struct Fetch {
        bool finished = false;
        QString path;
        QString error;
    };
    Fetch fetchInto(const QString &product, const QString &destDir) {
        Fetch r;
        UpdateChecker checker(base(), product, m_key.pub);
        bool checked = false;
        std::optional<UpdateManifest> manifest;
        checker.checkForUpdate([&](std::optional<UpdateManifest> m, QString e) {
            manifest = std::move(m); r.error = std::move(e); checked = true;
        });
        spinUntil(checked);
        if (!checked) return r;
        const auto art = manifest ? manifest->currentArtifact() : std::nullopt;
        if (!art) {
            r.finished = true;
            r.error = QStringLiteral("kein Artefakt: ") + r.error;
            return r;
        }
        r.error.clear();
        checker.downloadArtifact(*art, destDir, [&](QString p, QString e) {
            r.path = std::move(p); r.error = std::move(e); r.finished = true;
        });
        spinUntil(r.finished);
        return r;
    }

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

    // --- Ersetzen statt Überschreiben (QTMUX-147, Hub-Stand MAC-95) ----------
    // Bis QTMUX-147 öffnete der Downloader den Zielnamen mit WriteOnly|Truncate:
    // eine dort liegende Datei behielt ihr Inode (unter macOS samt einer
    // Quarantäne-Marke, die ein Browser gesetzt hatte), ein Symlink wurde bis in
    // sein Ziel beschrieben. Seitdem landen die Bytes in <Name>.XXXXXX.part und
    // erst ein geprüfter Download ersetzt den NAMEN. Die Regel hat sich
    // geändert; der Zweck der Wächter bleibt: der Aufrufer bekommt genau den
    // Installer, den das Manifest beschreibt — und sonst nichts verändert sich.

    // Ein vorhandener Name wird ersetzt: neue Bytes, neue Datei, kein Rest. Läuft
    // auf allen drei Plattformen — unter Windows ist das der Weg
    // MoveFileExW(REPLACE_EXISTING), den ein Download in einen leeren Ordner
    // (checkerVerifiesParsesAndDownloads) nie fährt.
    void downloadReplacesAnExistingFileOfTheSameName() {
        QTemporaryDir dest;
        QVERIFY(dest.isValid());
        const QString target = dest.path() + QStringLiteral("/QTmux-9.9.9.pkgtest");
        QVERIFY(writeFile(target, QByteArray("FRUEHERER-DOWNLOAD")));
#if !defined(Q_OS_WIN)
        const quint64 before = inodeOf(target);
        QVERIFY(before != 0);
#endif
        const Fetch r = fetchInto(QStringLiteral("qtmux"), dest.path());
        QVERIFY2(r.finished, "Download kam nicht zurück (Wache abgelaufen)");
        QVERIFY2(r.error.isEmpty(), qPrintable(r.error));
        QCOMPARE(r.path, target);
        QCOMPARE(contentOf(target), m_payload);
#if !defined(Q_OS_WIN)
        // Gleicher Inhalt käme auch beim Überschreiben heraus — erst das Inode
        // unterscheidet „ersetzt“ von „hineingeschrieben“.
        QVERIFY2(inodeOf(target) != before,
                 "Der Zielname trägt noch die alte Datei: überschrieben statt ersetzt");
#endif
        QCOMPARE(partFiles(dest.path()), QStringList());
    }

    // Ein Symlink am Zielnamen wird ersetzt; sein Ziel bleibt unberührt. Sonst
    // könnte ein Link im Download-Ordner ein Update an jede beschreibbare Stelle
    // lenken.
    void downloadReplacesASymlinkNotItsTarget() {
#if defined(Q_OS_WIN)
        QSKIP("QFile::link legt unter Windows eine .lnk-Verknüpfung an, keinen Symlink; "
              "ein echter Symlink braucht dort ein Recht, das ein Runner nicht zusichert");
#else
        QTemporaryDir dest, elsewhere;
        QVERIFY(dest.isValid() && elsewhere.isValid());
        const QString victim = elsewhere.path() + QStringLiteral("/fremd.bin");
        QVERIFY(writeFile(victim, QByteArray("FREMDE-DATEI")));
        const QString target = dest.path() + QStringLiteral("/QTmux-9.9.9.pkgtest");
        QVERIFY(QFile::link(victim, target));
        QVERIFY(QFileInfo(target).isSymLink());   // Positivkontrolle: der Link liegt
        const Fetch r = fetchInto(QStringLiteral("qtmux"), dest.path());
        QVERIFY2(r.finished, "Download kam nicht zurück (Wache abgelaufen)");
        QVERIFY2(r.error.isEmpty(), qPrintable(r.error));
        QCOMPARE(contentOf(victim), QByteArray("FREMDE-DATEI"));
        QVERIFY2(!QFileInfo(target).isSymLink(), "Der Link liegt noch am Zielnamen");
        QCOMPARE(contentOf(target), m_payload);
        QCOMPARE(partFiles(dest.path()), QStringList());
#endif
    }

    // Falscher SHA: der NEUE Download fliegt weg, ein ÄLTERER gleichen Namens
    // bleibt, wie er war. (Früher war er nach dem Truncate schon zerstört.)
    void shaMismatchLeavesAnOlderFileUntouched() {
        QTemporaryDir dest;
        QVERIFY(dest.isValid());
        const QString target = dest.path() + QStringLiteral("/QTmux-9.9.9.pkgtest");
        QVERIFY(writeFile(target, QByteArray("FRUEHERER-DOWNLOAD")));
        const Fetch r = fetchInto(QStringLiteral("shamismatch"), dest.path());
        QVERIFY2(r.finished, "Download kam nicht zurück (Wache abgelaufen)");
        QVERIFY(r.path.isEmpty());
        QVERIFY2(r.error.contains(QStringLiteral("sha256")), qPrintable(r.error));
        QCOMPARE(contentOf(target), QByteArray("FRUEHERER-DOWNLOAD"));
        QCOMPARE(partFiles(dest.path()), QStringList());
    }

    // Jemand hält die alte Datei offen (unter Windows z. B. ein noch laufender
    // Installer oder ein Virenscanner). Zusage: IMMER eine Antwort — Ersetzen
    // oder ein Fehler mit Grund —, nie ein Hänger, nie ein *.part-Rest, und wer
    // die alte Datei offen hat, liest weiter die alten Bytes. Welche Antwort
    // Windows gibt, hängt an den Freigabe-Flags des Haltenden; der Test
    // protokolliert sie, statt sie zu raten.
    void aFileHeldOpenGetsAnAnswerNeverAHang() {
        QTemporaryDir dest;
        QVERIFY(dest.isValid());
        const QString target = dest.path() + QStringLiteral("/QTmux-9.9.9.pkgtest");
        QVERIFY(writeFile(target, QByteArray("OFFEN-GEHALTEN")));
        QFile holder(target);
        QVERIFY(holder.open(QIODevice::ReadOnly));
        const Fetch r = fetchInto(QStringLiteral("qtmux"), dest.path());
        QVERIFY2(r.finished, "Download kam nicht zurück (Wache abgelaufen)");
        // Der Haltende sieht seine Datei unverändert (beim alten Überschreiben
        // las er hier den neuen Inhalt).
        QCOMPARE(holder.readAll(), QByteArray("OFFEN-GEHALTEN"));
        holder.close();
        if (r.error.isEmpty()) {
            qInfo("offen gehaltene Datei: Name ersetzt");
            QCOMPARE(r.path, target);
            QCOMPARE(contentOf(target), m_payload);
        } else {
            qInfo("offen gehaltene Datei: abgelehnt — %s", qPrintable(r.error));
            QVERIFY2(r.error.contains(QStringLiteral("cannot replace")), qPrintable(r.error));
            QVERIFY(r.path.isEmpty());
            QCOMPARE(contentOf(target), QByteArray("OFFEN-GEHALTEN"));
        }
#if !defined(Q_OS_WIN)
        // POSIX ersetzt einen Namen auch bei offenem Handle.
        QVERIFY2(r.error.isEmpty(), qPrintable(r.error));
#endif
        QCOMPARE(partFiles(dest.path()), QStringList());
    }

    // --- Reste eines harten Endes (Hub-Stand MAC-99) ---------------------------
    // Stirbt die App zwischen dem Anlegen von <Name>.XXXXXX.part und dem
    // Umbenennen (SIGKILL, Absturz, Strom weg), bleibt die Zwischendatei liegen —
    // ein Update-Paket wiegt 30–60 MB. Der nächste Download DESSELBEN Namens in
    // DENSELBEN Ordner räumt sie ab, und nur, was nachweislich so ein Rest ist:
    // genau das Muster, eine gewöhnliche Datei, älter als die Frist des Hubs
    // (10 min) und von keinem laufenden Download belegt. Jede Löschung steht im
    // Log. Beide Wächter laufen auf allen drei Plattformen.

    // Ein alter Rest verschwindet mit Logzeile; was ihm nur ähnlich sieht, bleibt.
    void aRemnantOfAnEarlierDownloadIsRemovedAndLogged() {
        QTemporaryDir dest;
        QVERIFY(dest.isValid());
        const QString name = QStringLiteral("QTmux-9.9.9.pkgtest");
        const auto at = [&](const QString &n) { return dest.path() + QLatin1Char('/') + n; };
        const QString remnant = name + QStringLiteral(".Ab3dE9.part");
        QVERIFY(writeFile(at(remnant), QByteArray(70000, 'r')));
        QVERIFY(age(at(remnant), 3600));
        // Positivkontrolle: der Rest ist wirklich alt (sonst bewiese sein
        // Verbleiben nichts über die Frist).
        QVERIFY(QFileInfo(at(remnant)).lastModified().secsTo(QDateTime::currentDateTime()) > 3000);
        // Ebenso alt, aber nicht unser: anderer Name, Beinahe-Treffer des Musters.
        const QStringList decoys{
            QStringLiteral("QTmux-9.9.8.pkgtest.Ab3dE9.part"),
            name + QStringLiteral(".Ab3dE.part"),
            name + QStringLiteral(".Ab3dE9.part.bak"),
        };
        for (const QString &d : decoys) {
            QVERIFY(writeFile(at(d), "fremd"));
            QVERIFY(age(at(d), 3600));
        }
        // Jung: womöglich der Download einer zweiten Instanz, die gerade beginnt.
        const QString young = name + QStringLiteral(".Young1.part");
        QVERIFY(writeFile(at(young), "wird geschrieben"));
        QStringList expected = decoys;
        expected << name << young;
#if !defined(Q_OS_WIN)
        QTemporaryDir elsewhere;
        QVERIFY(elsewhere.isValid());
        const QString linkTarget = elsewhere.path() + QStringLiteral("/fremd.bin");
        QVERIFY(writeFile(linkTarget, "nicht unseres"));
        QVERIFY(age(linkTarget, 3600));
        const QString link = name + QStringLiteral(".Lnk123.part");
        QVERIFY(QFile::link(linkTarget, at(link)));
        QVERIFY(QFileInfo(at(link)).isSymLink());
        expected << link;
#endif
        expected.sort();

        WarningLog log;
        const Fetch r = fetchInto(QStringLiteral("qtmux"), dest.path());
        QVERIFY2(r.finished, "Download kam nicht zurück (Wache abgelaufen)");
        QVERIFY2(r.error.isEmpty(), qPrintable(r.error));
        QCOMPARE(contentOf(at(name)), m_payload);

        QVERIFY2(!QFileInfo::exists(at(remnant)), "Der Rest des früheren Downloads liegt noch da");
        QVERIFY2(log.text().contains(QStringLiteral("removed download remnant"))
                     && log.text().contains(remnant),
                 qPrintable(QStringLiteral("Löschung nicht im Log: ") + log.text()));
        QCOMPARE(log.text().count(QStringLiteral("removed download remnant")), 1);
        QCOMPARE(entriesOf(dest.path()), expected);
#if !defined(Q_OS_WIN)
        QVERIFY2(QFileInfo(at(link)).isSymLink(), "Ein Link mit dem Musternamen wurde entfernt");
        QCOMPARE(contentOf(linkTarget), QByteArray("nicht unseres"));
#endif
        qInfo("Aufräum-Log: %s", qPrintable(log.text()));
    }

    // Ein alter Rest, den ein noch LEBENDER Download belegt (ein hängender
    // Download einer zweiten Instanz — der Downloader kennt keine Frist, das
    // Alter allein beweist also nichts), bleibt. Der Haltende spielt hier, was
    // der Hub selbst tut: POSIX ein flock() auf einem eigenen Deskriptor,
    // Windows eine offene Datei ohne FILE_SHARE_DELETE (so öffnet Qt). Erst
    // wenn er loslässt — wie nach seinem harten Ende —, räumt der nächste
    // Download ab (Positivkontrolle: nur die Belegung hat geschützt).
    void aPartHeldByALivingDownloadStays() {
        QTemporaryDir dest;
        QVERIFY(dest.isValid());
        const QString name = QStringLiteral("QTmux-9.9.9.pkgtest");
        const QString part = dest.path() + QLatin1Char('/') + name + QStringLiteral(".Live01.part");
        QVERIFY(writeFile(part, QByteArray(65536, 'x')));
        QVERIFY(age(part, 3600));
#if defined(Q_OS_WIN)
        QFile holder(part);
        QVERIFY(holder.open(QIODevice::ReadWrite));
#else
        const int holder = ::open(QFile::encodeName(part).constData(), O_RDONLY | O_CLOEXEC);
        QVERIFY(holder >= 0);
        QCOMPARE(::flock(holder, LOCK_EX | LOCK_NB), 0);
#endif
        {
            WarningLog log;
            const Fetch r = fetchInto(QStringLiteral("qtmux"), dest.path());
            QVERIFY2(r.finished, "Download kam nicht zurück (Wache abgelaufen)");
            QVERIFY2(r.error.isEmpty(), qPrintable(r.error));
            QVERIFY2(QFileInfo::exists(part),
                     "Das Aufräumen nahm die Datei eines Downloads, der noch läuft");
            QVERIFY2(!log.text().contains(QStringLiteral("removed download remnant")),
                     qPrintable(log.text()));
            qInfo("belegt: Log \"%s\"", qPrintable(log.text()));
        }
#if defined(Q_OS_WIN)
        holder.close();
#else
        ::close(holder);   // gibt das flock frei
#endif
        QVERIFY(QFileInfo::exists(part));
        QVERIFY(age(part, 3600));
        {
            WarningLog log;
            const Fetch r = fetchInto(QStringLiteral("qtmux"), dest.path());
            QVERIFY2(r.finished, "Download kam nicht zurück (Wache abgelaufen)");
            QVERIFY2(r.error.isEmpty(), qPrintable(r.error));
            QVERIFY2(!QFileInfo::exists(part), "Der freigegebene Rest überlebte den Download");
            QVERIFY2(log.text().contains(QStringLiteral("removed download remnant")),
                     qPrintable(log.text()));
        }
        QCOMPARE(entriesOf(dest.path()), QStringList{name});
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

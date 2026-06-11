#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include "SftpClient.h"

using namespace qtmux;

class TestSftp : public QObject {
    Q_OBJECT
private slots:
    void parsesListing();
    void parsesLinuxStyleAndSymlink();
    void liveConnectListTransfer();
};

// Reale sftp `ls -la`-Ausgabe (inkl. Echo-Zeile + Prompt + "."/"..") wird korrekt
// in Einträge zerlegt; Rauschen wird übersprungen.
void TestSftp::parsesListing() {
    const QString out =
        "ls -la\r\n"
        "drwxr-xr-x    ? nobser   wheel         384 Jun 11 21:24 .\r\n"
        "drwxrwxrwt    ? root     wheel        2688 Jun 11 21:24 ..\r\n"
        "-rw-r--r--    ? nobser   wheel          32 Jun 11 21:24 remote_sample.txt\r\n"
        "drwxr-xr-x    ? nobser   wheel          64 Jun 11 21:24 subdir\r\n"
        "Couldn't stat remote file: No such file or directory\r\n"
        "sftp> ";
    const QVariantList e = SftpClient::parseListing(out);
    QCOMPARE(e.size(), 2);   // "."/".." + Echo + Fehlerzeile + Prompt herausgefiltert

    const QVariantMap f = e.at(0).toMap();
    QCOMPARE(f.value("name").toString(), QStringLiteral("remote_sample.txt"));
    QCOMPARE(f.value("size").toLongLong(), 32LL);
    QCOMPARE(f.value("isDir").toBool(), false);

    const QVariantMap d = e.at(1).toMap();
    QCOMPARE(d.value("name").toString(), QStringLiteral("subdir"));
    QCOMPARE(d.value("isDir").toBool(), true);
}

// Linux-Stil (numerische Link-Anzahl, Jahres-Datum) + Symlink (Name ohne "-> Ziel").
void TestSftp::parsesLinuxStyleAndSymlink() {
    const QString out =
        "-rw-r--r-- 1 user group 1234 Jan  2  2023 old file.txt\r\n"
        "lrwxrwxrwx 1 user group   11 Jan  2 10:00 link -> /etc/hosts\r\n";
    const QVariantList e = SftpClient::parseListing(out);
    QCOMPARE(e.size(), 2);

    const QVariantMap f = e.at(0).toMap();
    QCOMPARE(f.value("name").toString(), QStringLiteral("old file.txt"));  // Name mit Leerzeichen
    QCOMPARE(f.value("size").toLongLong(), 1234LL);

    const QVariantMap l = e.at(1).toMap();
    QCOMPARE(l.value("name").toString(), QStringLiteral("link"));          // "-> Ziel" entfernt
    QCOMPARE(l.value("isLink").toBool(), true);
}

// Echter End-to-End-Test gegen einen SFTP-Server. Übersprungen, sofern nicht
// QTMUX_SFTP_HOST gesetzt ist (so läuft die CI/andere Umgebung ohne Server).
// Env: QTMUX_SFTP_HOST, _PORT (Default 22), _USER, _KEY (Identity), _DIR (Remote-Pfad
// mit Schreibrecht). Testet connect → list → cd → download → upload.
void TestSftp::liveConnectListTransfer() {
    const QByteArray host = qgetenv("QTMUX_SFTP_HOST");
    if (host.isEmpty()) QSKIP("QTMUX_SFTP_HOST nicht gesetzt – Live-SFTP-Test übersprungen");
    const int port = qEnvironmentVariableIntValue("QTMUX_SFTP_PORT");
    const QString user = qEnvironmentVariable("QTMUX_SFTP_USER");
    const QString key = qEnvironmentVariable("QTMUX_SFTP_KEY");
    const QString dir = qEnvironmentVariable("QTMUX_SFTP_DIR");

    SftpClient c;
    QSignalSpy errSpy(&c, &SftpClient::error);
    c.connectTo(QString::fromUtf8(host), port, user, key, QString());
    QTRY_VERIFY_WITH_TIMEOUT(c.isConnected() || errSpy.count() > 0, 20000);
    QVERIFY2(errSpy.isEmpty(), errSpy.isEmpty() ? "" :
             qPrintable(errSpy.first().first().toString()));
    // Nach Connect folgt automatisch pwd + Auflistung.
    QTRY_VERIFY_WITH_TIMEOUT(!c.currentPath().isEmpty() && !c.isBusy(), 20000);

    if (!dir.isEmpty()) {
        c.cd(dir.startsWith('/') ? dir : dir);   // absoluter Pfad in der Env erwartet
        QTRY_VERIFY_WITH_TIMEOUT(!c.isBusy(), 20000);
        QCOMPARE(c.currentPath(), dir);
    }

    // Upload einer lokalen Testdatei, dann Download in einen frischen Ordner.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString upName = QStringLiteral("qtmux_sftp_probe.txt");
    const QString upPath = tmp.filePath(upName);
    { QFile f(upPath); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("qtmux-sftp-roundtrip"); }

    QSignalSpy xfer(&c, &SftpClient::transferFinished);
    c.upload(upPath);
    QTRY_VERIFY_WITH_TIMEOUT(xfer.count() >= 1, 20000);
    QVERIFY2(xfer.last().at(0).toBool(), "Upload fehlgeschlagen");

    // Hochgeladene Datei taucht in der (aufgefrischten) Liste auf.
    QTRY_VERIFY_WITH_TIMEOUT(!c.isBusy(), 20000);
    bool found = false;
    for (const QVariant &v : c.entries())
        if (v.toMap().value("name").toString() == upName) found = true;
    QVERIFY2(found, "Hochgeladene Datei nicht in der Liste");

    const QString dlDir = tmp.filePath(QStringLiteral("dl"));
    QDir().mkpath(dlDir);
    c.download(upName, dlDir);
    QTRY_VERIFY_WITH_TIMEOUT(xfer.count() >= 2, 20000);
    QVERIFY2(xfer.last().at(0).toBool(), "Download fehlgeschlagen");
    QFile dl(dlDir + "/" + upName);
    QVERIFY(dl.open(QIODevice::ReadOnly));
    QCOMPARE(dl.readAll(), QByteArray("qtmux-sftp-roundtrip"));

    c.close();
}

QTEST_MAIN(TestSftp)
#include "tst_sftp.moc"

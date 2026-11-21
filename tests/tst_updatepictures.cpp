// Das Bildziel qtmux-bilder (src/app/UpdatePicture.h): Bilder der Update-Oberfläche
// aus den Szenen des Hubs (appupdate::UpdateShowcase) und zwei QTmux-eigenen.
//
// Gefahren wird eine Auswahl, je Fläche mindestens eine Szene: Dialog (3, 5, 14),
// Proxy-Anmeldung (15), Hinweis in der Kopfleiste bei 720 px (hint-avail-narrow),
// Einstellungsfenster (settings-manual) und die beiden QTmux-Szenen. Gefordert je
// Szene: Exit 0, ein Bild mit Inhalt (lesbar, Mindestmaße, nicht einfarbig, mindestens
// acht Farben; die Dateigröße steht nur in der Meldung), kein QML-Fehler im Log. Dazu: Eine unbekannte
// Szene und ein Aufruf ohne --update-picture enden mit Exit 2 und ohne Bild.
//
// Das Programm entsteht nur mit QTMUX_UPDATE_PICTURES; ohne den Schalter gibt es
// diesen Test nicht (tests/CMakeLists.txt). Dass die App selbst den Baustein nicht
// enthält, misst test_release_has_no_showcase.
//
// Diagnose (ein CI-Lauf endete unter Windows rot ohne jede Ausgabe): Die Ausgabe ist
// ungepuffert, je Kindprozess steht eine Zeile „SZENE <name> exit=… status=… dauer=…“
// auf stderr, und sein Log (appupdate.*.debug) liegt als Datei unter
// QTMUX_PICTURE_LOG_DIR. ctest ruft den Test über tests/RunAndReport.cmake auf, das bei
// Rot Exit-Code bzw. Ausnahmekennung, die QTest-Datei und diese Logs druckt. Am Anfang
// steht, wohin „localhost“ auflöst (Szene 15 setzt den Proxy unter diesem Namen).

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QHostInfo>
#include <QImage>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSet>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTest>

#include <cstdio>

namespace {

// Vor main(): Geht der Prozess ab, steht alles bis dahin Geschriebene schon in der Pipe.
struct UnbufferedOutput {
    UnbufferedOutput()
    {
        std::setvbuf(stdout, nullptr, _IONBF, 0);
        std::setvbuf(stderr, nullptr, _IONBF, 0);
    }
} const s_unbuffered;

struct Run {
    bool finished = false;
    int code = -1;
    QProcess::ExitStatus status = QProcess::NormalExit;
    qint64 ms = 0;
    QString log;
    QString out;
};

Run runPicture(const QTemporaryDir &work, const QString &label, const QStringList &args,
               int timeoutMs = 60000)
{
    static int counter = 0;
    QTcpServer probe;
    probe.listen(QHostAddress::LocalHost, 0);
    const quint16 port = probe.serverPort();
    probe.close();

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    env.insert(QStringLiteral("QTMUX_NO_GPU"), QStringLiteral("1"));
    env.insert(QStringLiteral("QTMUX_MCP_PORT"), QString::number(port));
    env.insert(QStringLiteral("QTMUX_PROFILE"),
               QStringLiteral("update-pictures-%1-%2")
                   .arg(QCoreApplication::applicationPid())
                   .arg(++counter));
    env.insert(QStringLiteral("QT_LOGGING_RULES"), QStringLiteral("appupdate.*.debug=true"));
    // Unter Windows schreibt Qt ohne diese beiden nur an den Debugger, nicht in die Pipe.
    env.insert(QStringLiteral("QT_ASSUME_STDERR_HAS_CONSOLE"), QStringLiteral("1"));
    env.insert(QStringLiteral("QT_FORCE_STDERR_LOGGING"), QStringLiteral("1"));

    QProcess child;
    child.setProcessEnvironment(env);
    child.setProcessChannelMode(QProcess::MergedChannels);
    const QString dir = work.filePath(QStringLiteral("run-%1").arg(counter));
    QDir().mkpath(dir);
    Run r;
    QElapsedTimer clock;
    clock.start();
    child.start(QStringLiteral(QTMUX_PICTURE_BINARY),
                QStringList{QStringLiteral("--selftest-update"), dir} + args);
    if (!child.waitForStarted(20000)) {
        r.out = QStringLiteral("Start gescheitert: ") + child.errorString();
    } else {
        r.finished = child.waitForFinished(timeoutMs);
        if (!r.finished) {
            child.kill();
            child.waitForFinished(5000);
        }
        r.code = child.exitCode();
        r.status = child.exitStatus();
        r.out = QString::fromLocal8Bit(child.readAll());
    }
    r.ms = clock.elapsed();

    QString safe = label;
    safe.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")), QStringLiteral("_"));
    r.log = QStringLiteral(QTMUX_PICTURE_LOG_DIR "/%1-%2.log").arg(counter).arg(safe);
    QFile f(r.log);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.write(r.out.toLocal8Bit());

    // Unter Windows trägt ein abgestürzter Kindprozess die Ausnahmekennung (NTSTATUS,
    // z. B. 0xC0000005) als Exit-Code; darum auch hexadezimal.
    std::fprintf(stderr, "SZENE %s exit=%d (0x%08X) status=%s beendet=%s dauer=%lld ms log=%s\n",
                 qPrintable(label), r.code, static_cast<unsigned>(r.code),
                 r.status == QProcess::CrashExit ? "absturz" : "normal",
                 r.finished ? "ja" : "nein-frist", static_cast<long long>(r.ms),
                 qPrintable(r.log));
    return r;
}

// Ist das Bild eines mit Inhalt? Leer = ja, sonst der Grund (mit Dateigröße).
// Die Dateigröße allein taugt nicht: Ohne Schriften (Windows-Runner, offscreen) fiel eine
// flächige, aber richtige Szene unter 10 000 Byte.
QString pictureProblem(const QString &png)
{
    constexpr int kMinWidth = 300;
    constexpr int kMinHeight = 200;
    constexpr int kMinColors = 8;
    const qint64 bytes = QFileInfo(png).size();
    QImage img;
    if (!img.load(png))
        return QStringLiteral("kein lesbares Bild (%1 Byte)").arg(bytes);
    if (img.width() < kMinWidth || img.height() < kMinHeight)
        return QStringLiteral("zu klein: %1x%2 (%3 Byte)")
            .arg(img.width()).arg(img.height()).arg(bytes);
    QSet<QRgb> colors;
    for (int y = 0; y < img.height() && colors.size() < kMinColors; ++y)
        for (int x = 0; x < img.width() && colors.size() < kMinColors; ++x)
            colors.insert(img.pixel(x, y));
    if (colors.size() == 1)
        return QStringLiteral("einfarbig (%1x%2, %3 Byte)")
            .arg(img.width()).arg(img.height()).arg(bytes);
    if (colors.size() < kMinColors)
        return QStringLiteral("nur %1 Farben (%2x%3, %4 Byte)")
            .arg(colors.size()).arg(img.width()).arg(img.height()).arg(bytes);
    return {};
}

} // namespace

class TestUpdatePictures : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Logs des letzten Laufs entfernen: nur einzelne *.log-Dateien im eigenen Ordner,
        // nie den Ordner rekursiv (ein leerer Pfad hieße sonst Arbeitsverzeichnis).
        const QString logDir = QStringLiteral(QTMUX_PICTURE_LOG_DIR);
        QVERIFY(logDir.endsWith(QLatin1String("test_updatepictures_logs")));
        QVERIFY(QDir().mkpath(logDir));
        const QDir logs(logDir);
        for (const QString &old : logs.entryList({QStringLiteral("*.log")}, QDir::Files))
            QFile::remove(logs.filePath(old));
        // Wohin löst „localhost“ hier auf? (Der Szenen-Server lauscht auf 127.0.0.1.)
        const QHostInfo info = QHostInfo::fromName(QStringLiteral("localhost"));
        QStringList addrs;
        for (const QHostAddress &a : info.addresses()) addrs << a.toString();
        std::fprintf(stderr, "DIAGNOSE localhost -> [%s] fehler=%s\n",
                     qPrintable(addrs.join(QStringLiteral(", "))),
                     qPrintable(info.errorString()));
        // Proxy-Variablen der Umgebung: nur ob gesetzt, nie ihr Wert.
        for (const char *v : {"http_proxy", "HTTP_PROXY", "https_proxy", "HTTPS_PROXY",
                              "all_proxy", "ALL_PROXY", "no_proxy", "NO_PROXY"})
            std::fprintf(stderr, "DIAGNOSE %s=%s\n", v,
                         qEnvironmentVariableIsSet(v) ? "gesetzt" : "-");
    }

    void eachSurfaceBecomesAPicture_data()
    {
        QTest::addColumn<QString>("scene");
        QTest::addColumn<QString>("theme");
        QTest::addColumn<QString>("language");
        QTest::addColumn<QString>("logMust");
        QTest::newRow("3 hell de") << "3" << "light" << "de" << "";
        QTest::newRow("5 dunkel en") << "5" << "dark" << "en" << "";
        QTest::newRow("14 hell en") << "14" << "light" << "en" << "install: manual";
        QTest::newRow("15 dunkel de") << "15" << "dark" << "de" << "";
        QTest::newRow("hint-avail-narrow") << "hint-avail-narrow" << "light" << "de" << "";
        QTest::newRow("settings-manual") << "settings-manual" << "dark" << "de" << "";
        QTest::newRow("qtmux-other-instance") << "qtmux-other-instance" << "light" << "de"
                                              << "install: manual other-instance-running";
        QTest::newRow("qtmux-settings-port") << "qtmux-settings-port" << "light" << "en" << "";
    }

    void eachSurfaceBecomesAPicture()
    {
        QFETCH(QString, scene);
        QFETCH(QString, theme);
        QFETCH(QString, language);
        QFETCH(QString, logMust);
        QTemporaryDir work;
        QVERIFY(work.isValid());
        const QString png = work.filePath(QStringLiteral("scene.png"));
        const Run r = runPicture(work, QString::fromLatin1(QTest::currentDataTag()),
                                 {QStringLiteral("--update-picture"), scene,
                                  QStringLiteral("--screenshot"), png,
                                  QStringLiteral("--picture-theme"), theme,
                                  QStringLiteral("--picture-language"), language});
        QVERIFY2(r.finished, qPrintable(r.out));
        QVERIFY2(r.status == QProcess::NormalExit && r.code == 0, qPrintable(r.out));
        static const QRegularExpression qmlError(QStringLiteral(
            "failed to load component|Type \\w+ unavailable|Cannot override FINAL|"
            "ReferenceError|TypeError|is not defined|qrc:/qt/qml/QTmux/\\S+: (?!QML )"));
        const auto hit = qmlError.match(r.out);
        QVERIFY2(!hit.hasMatch(), qPrintable(QStringLiteral("QML-Fehler „%1“:\n%2")
                                                 .arg(hit.captured(0), r.out)));
        QVERIFY2(r.out.contains(QStringLiteral("qtmux-bilder: %1 rc=0").arg(png)), qPrintable(r.out));
        if (!logMust.isEmpty()) QVERIFY2(r.out.contains(logMust), qPrintable(r.out));
        const QString problem = pictureProblem(png);
        QVERIFY2(problem.isEmpty(), qPrintable(problem + QLatin1Char('\n') + r.out));
    }

    void aBlankPictureIsRefused()
    {
        // Gegenprobe der Bildprüfung: leere Datei, einfarbig, zu klein, zu wenige Farben.
        QTemporaryDir work;
        QVERIFY(work.isValid());
        const QString empty = work.filePath(QStringLiteral("empty.png"));
        QFile f(empty);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        QVERIFY(pictureProblem(empty).startsWith(QLatin1String("kein lesbares Bild")));

        QImage plain(720, 720, QImage::Format_RGB32);
        plain.fill(QColor(250, 250, 250));
        const QString plainPng = work.filePath(QStringLiteral("plain.png"));
        QVERIFY(plain.save(plainPng));
        QVERIFY2(pictureProblem(plainPng).startsWith(QLatin1String("einfarbig")),
                 qPrintable(pictureProblem(plainPng)));

        QImage tiny(40, 40, QImage::Format_RGB32);
        for (int y = 0; y < 40; ++y)
            for (int x = 0; x < 40; ++x) tiny.setPixel(x, y, qRgb(x * 6, y * 6, 128));
        const QString tinyPng = work.filePath(QStringLiteral("tiny.png"));
        QVERIFY(tiny.save(tinyPng));
        QVERIFY(pictureProblem(tinyPng).startsWith(QLatin1String("zu klein")));

        QImage two(720, 720, QImage::Format_RGB32);
        two.fill(QColor(250, 250, 250));
        for (int y = 0; y < 360; ++y)
            for (int x = 0; x < 720; ++x) two.setPixel(x, y, qRgb(30, 30, 30));
        const QString twoPng = work.filePath(QStringLiteral("two.png"));
        QVERIFY(two.save(twoPng));
        QVERIFY(pictureProblem(twoPng).startsWith(QLatin1String("nur 2 Farben")));

        QImage rich(720, 720, QImage::Format_RGB32);
        for (int y = 0; y < 720; ++y)
            for (int x = 0; x < 720; ++x) rich.setPixel(x, y, qRgb(x % 256, y % 256, 90));
        const QString richPng = work.filePath(QStringLiteral("rich.png"));
        QVERIFY(rich.save(richPng));
        QCOMPARE(pictureProblem(richPng), QString());
    }

    void anUnknownSceneOrNoSceneWritesNothing()
    {
        QTemporaryDir work;
        QVERIFY(work.isValid());
        const QString none = work.filePath(QStringLiteral("none.png"));
        const Run bad = runPicture(work, QStringLiteral("unbekannte-szene"),
                                   {QStringLiteral("--update-picture"), QStringLiteral("no-such"),
                                    QStringLiteral("--screenshot"), none});
        QVERIFY2(bad.finished, qPrintable(bad.out));
        QCOMPARE(bad.code, 2);
        QVERIFY2(bad.out.contains(QLatin1String("unbekannte Szene")), qPrintable(bad.out));
        QVERIFY(!QFileInfo::exists(none));

        // Ohne Szene ist das Programm nichts: kein Fenster, keine App.
        const Run noScene = runPicture(work, QStringLiteral("ohne-szene"),
                                       {QStringLiteral("--screenshot"), none});
        QVERIFY2(noScene.finished, qPrintable(noScene.out));
        QCOMPARE(noScene.code, 2);
        QVERIFY(!QFileInfo::exists(none));
    }
};

QTEST_GUILESS_MAIN(TestUpdatePictures)
#include "tst_updatepictures.moc"

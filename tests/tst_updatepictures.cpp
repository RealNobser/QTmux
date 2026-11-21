// Das Bildziel qtmux-bilder (src/app/UpdatePicture.h): Bilder der Update-Oberfläche
// aus den Szenen des Hubs (appupdate::UpdateShowcase) und zwei QTmux-eigenen.
//
// Gefahren wird eine Auswahl, je Fläche mindestens eine Szene: Dialog (3, 5, 14),
// Proxy-Anmeldung (15), Hinweis in der Kopfleiste bei 720 px (hint-avail-narrow),
// Einstellungsfenster (settings-manual) und die beiden QTmux-Szenen. Gefordert je
// Szene: Exit 0, ein Bild mit Inhalt, kein QML-Fehler im Log. Dazu: Eine unbekannte
// Szene und ein Aufruf ohne --update-picture enden mit Exit 2 und ohne Bild.
//
// Das Programm entsteht nur mit QTMUX_UPDATE_PICTURES; ohne den Schalter gibt es
// diesen Test nicht (tests/CMakeLists.txt). Dass die App selbst den Baustein nicht
// enthält, misst test_release_has_no_showcase.

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTest>

namespace {

struct Run {
    bool finished = false;
    int code = -1;
    QString out;
};

Run runPicture(const QTemporaryDir &work, const QStringList &args, int timeoutMs = 60000)
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
    env.insert(QStringLiteral("QT_LOGGING_RULES"), QStringLiteral("appupdate.*.info=true"));

    QProcess child;
    child.setProcessEnvironment(env);
    child.setProcessChannelMode(QProcess::MergedChannels);
    const QString dir = work.filePath(QStringLiteral("run-%1").arg(counter));
    QDir().mkpath(dir);
    child.start(QStringLiteral(QTMUX_PICTURE_BINARY),
                QStringList{QStringLiteral("--selftest-update"), dir} + args);
    Run r;
    if (!child.waitForStarted(20000)) {
        r.out = child.errorString();
        return r;
    }
    r.finished = child.waitForFinished(timeoutMs);
    if (!r.finished) {
        child.kill();
        child.waitForFinished(5000);
    }
    r.code = child.exitCode();
    r.out = QString::fromLocal8Bit(child.readAll());
    return r;
}

} // namespace

class TestUpdatePictures : public QObject {
    Q_OBJECT

private slots:
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
        const Run r = runPicture(work, {QStringLiteral("--update-picture"), scene,
                                        QStringLiteral("--screenshot"), png,
                                        QStringLiteral("--picture-theme"), theme,
                                        QStringLiteral("--picture-language"), language});
        QVERIFY2(r.finished, qPrintable(r.out));
        QVERIFY2(r.code == 0, qPrintable(r.out));
        static const QRegularExpression qmlError(QStringLiteral(
            "failed to load component|Type \\w+ unavailable|Cannot override FINAL|"
            "ReferenceError|TypeError|is not defined|qrc:/qt/qml/QTmux/\\S+: (?!QML )"));
        const auto hit = qmlError.match(r.out);
        QVERIFY2(!hit.hasMatch(), qPrintable(QStringLiteral("QML-Fehler „%1“:\n%2")
                                                 .arg(hit.captured(0), r.out)));
        QVERIFY2(r.out.contains(QStringLiteral("qtmux-bilder: %1 rc=0").arg(png)), qPrintable(r.out));
        if (!logMust.isEmpty()) QVERIFY2(r.out.contains(logMust), qPrintable(r.out));
        QVERIFY2(QFileInfo(png).size() > 10000, qPrintable(r.out));
    }

    void anUnknownSceneOrNoSceneWritesNothing()
    {
        QTemporaryDir work;
        QVERIFY(work.isValid());
        const QString none = work.filePath(QStringLiteral("none.png"));
        const Run bad = runPicture(work, {QStringLiteral("--update-picture"),
                                          QStringLiteral("no-such"),
                                          QStringLiteral("--screenshot"), none});
        QVERIFY2(bad.finished, qPrintable(bad.out));
        QCOMPARE(bad.code, 2);
        QVERIFY2(bad.out.contains(QLatin1String("unbekannte Szene")), qPrintable(bad.out));
        QVERIFY(!QFileInfo::exists(none));

        // Ohne Szene ist das Programm nichts: kein Fenster, keine App.
        const Run noScene = runPicture(work, {QStringLiteral("--screenshot"), none});
        QVERIFY2(noScene.finished, qPrintable(noScene.out));
        QCOMPARE(noScene.code, 2);
        QVERIFY(!QFileInfo::exists(none));
    }
};

QTEST_GUILESS_MAIN(TestUpdatePictures)
#include "tst_updatepictures.moc"

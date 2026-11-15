// QTMUX-151: Rückfrage vor dem Öffnen ausführbarer Dateien — die Regel je Plattform.
// Alle drei Plattformen werden auf JEDEM Host geprüft (die Regel ist reine Zeichenketten- und
// Bit-Logik); `factsFor` und die Kurzform prüfen gegen echte Dateien in einem Temp-Ordner.
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "ExecutableTarget.h"

using namespace ExecutableTarget;
using FileManagerReveal::Platform;

class TestExecutableTarget : public QObject {
    Q_OBJECT

    static bool ask(Platform p, const QString &path, bool isDir = false, bool exec = false,
                    const QString &pathExt = QString(), const QString &resolved = QString()) {
        Facts f;
        f.isDir = isDir;
        f.anyExecBit = exec;
        f.resolved = resolved;
        return asksBeforeOpen(path, f, p, pathExt);
    }

private slots:
    // --- Windows: der Name entscheidet --------------------------------------------------------
    void windowsSuffixes_data() {
        QTest::addColumn<QString>("suffix");
        // Jede Endung einzeln: Fällt eine aus der Liste, wird genau ihre Zeile rot.
        for (const char *s : {".com", ".exe", ".bat", ".cmd", ".vbs", ".vbe", ".js", ".jse", ".wsf",
                              ".wsh", ".msc", ".msi", ".msp", ".msix", ".msixbundle", ".appx",
                              ".appxbundle", ".appinstaller", ".application", ".appref-ms", ".gadget",
                              ".diagcab", ".iso", ".img", ".vhd", ".vhdx", ".lnk", ".url", ".pif",
                              ".settingcontent-ms", ".website", ".library-ms", ".search-ms",
                              ".searchconnector-ms", ".scr", ".cpl", ".xll", ".ps1", ".hta", ".jar",
                              ".jnlp", ".wsc", ".sct", ".chm", ".py", ".pyw", ".ws", ".psc1", ".reg"})
            QTest::newRow(s) << QString::fromLatin1(s);
    }
    void windowsSuffixes() {
        QFETCH(QString, suffix);
        QVERIFY(ask(Platform::Windows, QStringLiteral("C:/Users/a/werkzeug") + suffix));
        // Auch mit leerem oder fremdem PATHEXT: Die eingebaute Liste gilt immer.
        QVERIFY(ask(Platform::Windows, QStringLiteral("C:\\Users\\a\\werkzeug") + suffix, false, false,
                    QStringLiteral(".XYZ")));
    }

    void windowsDetails() {
        const Platform w = Platform::Windows;
        // Schreibweise der Endung ist beliebig.
        QVERIFY(ask(w, QStringLiteral("C:/a/WERKZEUG.BAT")));
        QVERIFY(ask(w, QStringLiteral("C:/a/Setup.Exe")));
        // Nur der letzte Abschnitt zählt, in beide Richtungen.
        QVERIFY(ask(w, QStringLiteral("C:/a/bericht.pdf.exe")));
        QVERIFY(!ask(w, QStringLiteral("C:/a/werkzeug.exe.txt")));
        // Punkte und Leerzeichen am Ende ignoriert Windows beim Öffnen.
        QVERIFY(ask(w, QStringLiteral("C:/a/werkzeug.bat.")));
        QVERIFY(ask(w, QStringLiteral("C:/a/werkzeug.bat ")));
        QVERIFY(ask(w, QStringLiteral("C:/a/werkzeug.bat. .")));
        // Gewöhnliche Dateien fragen nicht.
        for (const char *p : {"C:/a/notiz.txt", "C:/a/bild.png", "C:/a/bericht.pdf", "C:/a/quelle.cpp",
                              "C:/a/.gitignore", "C:/a.b/ordner.exe/notiz.txt"})
            QVERIFY2(!ask(w, QString::fromLatin1(p)), p);
        // Eine Datei, die nur aus der Endung besteht, führt Windows genauso aus.
        QVERIFY(ask(w, QStringLiteral("C:/a/.bat")));
        // Ohne Endung fragen: Die Shell hängt beim Öffnen selbst eine Programm-Endung an.
        QVERIFY(ask(w, QStringLiteral("C:/a/werkzeug")));
        QVERIFY(ask(w, QStringLiteral("C:/a/LIESMICH")));
        QVERIFY(ask(w, QStringLiteral("C:/a/werkzeug.")));                       // der Punkt zählt nicht
        // Ein Punkt im Ordnernamen ist keine Endung der Datei: `notiz` hat keine.
        QVERIFY(ask(w, QStringLiteral("C:\\a\\ordner.txt\\notiz")));
        QVERIFY(ask(w, QStringLiteral("C:/a/ordner.txt/notiz")));
        // Ein benannter Datenstrom: Die Endung sagt nicht, was geöffnet wird.
        QVERIFY(ask(w, QStringLiteral("C:/a/notiz.txt:versteckt")));
        QVERIFY(ask(w, QStringLiteral("C:/a/werkzeug.exe::$DATA")));
        QVERIFY(ask(w, QStringLiteral("C:\\a\\bild.png:x.txt")));
        // Verzeichnisse fragen nie, mit oder ohne Endung.
        QVERIFY(!ask(w, QStringLiteral("C:/a/ordner.exe"), true));
        QVERIFY(!ask(w, QStringLiteral("C:/a/ordner"), true));
        // Außer es ist eine Verknüpfung: Qt meldet für eine `.lnk` auf einen Ordner ein Verzeichnis.
        for (const char *p : {"C:/a/Ordner.lnk", "C:/a/Seite.URL", "C:/a/alt.pif"})
            QVERIFY2(ask(w, QString::fromLatin1(p), true), p);
        // Ein Ausführungsrecht hat unter Windows keine Bedeutung.
        QVERIFY(!ask(w, QStringLiteral("C:/a/notiz.txt"), false, true));
    }

    void windowsPathExtAdds() {
        const Platform w = Platform::Windows;
        const QString ext = QStringLiteral(".COM;.EXE;.BAT;.CMD;.PL; .RB ;");
        QVERIFY(ask(w, QStringLiteral("C:/a/skript.pl"), false, false, ext));
        QVERIFY(ask(w, QStringLiteral("C:/a/skript.rb"), false, false, ext));
        QVERIFY(!ask(w, QStringLiteral("C:/a/skript.pl")));                      // ohne die Variable
        QVERIFY(!ask(w, QStringLiteral("C:/a/notiz.txt"), false, false, ext));
        // PATHEXT ist eine Windows-Sache.
        QVERIFY(!ask(Platform::MacOS, QStringLiteral("/a/skript.pl"), false, false, ext));
        QVERIFY(!ask(Platform::Linux, QStringLiteral("/a/skript.pl"), false, false, ext));
    }

    // --- macOS: Endungen (auch für Bündel) und Ausführungsrecht ------------------------------
    void macosSuffixes_data() {
        QTest::addColumn<QString>("suffix");
        for (const char *s : {".app", ".command", ".tool", ".terminal", ".pkg", ".mpkg", ".prefpane",
                              ".saver", ".workflow", ".action", ".fileloc", ".inetloc", ".webloc",
                              ".dmg", ".jar"})
            QTest::newRow(s) << QString::fromLatin1(s);
    }
    void macosSuffixes() {
        QFETCH(QString, suffix);
        const Platform m = Platform::MacOS;
        QVERIFY(ask(m, QStringLiteral("/Users/a/Werkzeug") + suffix));
        // Als Bündel ist es ein Verzeichnis, und gefragt wird trotzdem.
        QVERIFY(ask(m, QStringLiteral("/Users/a/Werkzeug") + suffix, true, true));
        QVERIFY(ask(m, QStringLiteral("/Users/a/Werkzeug") + suffix.toUpper(), true, true));
    }

    void macosDetails() {
        const Platform m = Platform::MacOS;
        QVERIFY(ask(m, QStringLiteral("/Applications/Rechner.app/"), true));     // mit Trenner am Ende
        QVERIFY(ask(m, QStringLiteral("/Library/PreferencePanes/Etwas.prefPane"), true, true));
        // Jede Datei mit Ausführungsrecht, gleich wie sie heißt.
        QVERIFY(ask(m, QStringLiteral("/Users/a/skript"), false, true));
        QVERIFY(ask(m, QStringLiteral("/Users/a/notiz.txt"), false, true));
        // Gewöhnliche Dateien und Ordner fragen nicht; ein Ordner hat immer ein Ausführungsrecht.
        QVERIFY(!ask(m, QStringLiteral("/Users/a/notiz.txt")));
        QVERIFY(!ask(m, QStringLiteral("/Users/a/skript")));
        QVERIFY(!ask(m, QStringLiteral("/Users/a/Projekte"), true, true));
        QVERIFY(!ask(m, QStringLiteral("/Users/a/Projekte.v2"), true, true));
        // Die Datei IN einem Bündel ist nicht das Bündel.
        QVERIFY(!ask(m, QStringLiteral("/Applications/Rechner.app/Contents/Info.plist")));
        // Endungen der anderen Plattformen zählen hier nicht.
        QVERIFY(!ask(m, QStringLiteral("/Users/a/werkzeug.bat")));
        QVERIFY(!ask(m, QStringLiteral("/Users/a/starter.desktop")));
        QVERIFY(!ask(m, QStringLiteral("/Users/a/werkzeug")));                   // ohne Endung, ohne Recht
    }

    // --- Linux: Ausführungsrecht und Endungen -------------------------------------------------
    void linuxSuffixes_data() {
        QTest::addColumn<QString>("suffix");
        for (const char *s : {".desktop", ".appimage", ".run", ".sh", ".jar", ".deb", ".rpm",
                              ".flatpakref", ".flatpak", ".snap"})
            QTest::newRow(s) << QString::fromLatin1(s);
    }
    void linuxSuffixes() {
        QFETCH(QString, suffix);
        QVERIFY(ask(Platform::Linux, QStringLiteral("/home/a/Werkzeug") + suffix));
        QVERIFY(ask(Platform::Linux, QStringLiteral("/home/a/Werkzeug") + suffix.toUpper()));
        // Ein Verzeichnis dieses Namens ist unter Linux nur ein Verzeichnis.
        QVERIFY(!ask(Platform::Linux, QStringLiteral("/home/a/Werkzeug") + suffix, true, true));
    }

    void linuxDetails() {
        const Platform l = Platform::Linux;
        QVERIFY(ask(l, QStringLiteral("/home/a/skript"), false, true));
        QVERIFY(ask(l, QStringLiteral("/home/a/notiz.txt"), false, true));
        QVERIFY(!ask(l, QStringLiteral("/home/a/notiz.txt")));
        QVERIFY(!ask(l, QStringLiteral("/home/a/skript")));
        QVERIFY(!ask(l, QStringLiteral("/home/a/projekte"), true, true));
        QVERIFY(!ask(l, QStringLiteral("/home/a/Rechner.app"), true, true));
        QVERIFY(!ask(l, QStringLiteral("/home/a/werkzeug.command")));
        QVERIFY(!ask(l, QStringLiteral("/home/a/werkzeug.bat")));
    }

    // --- Verknüpfungen: Der Name sagt nicht, was geöffnet wird ------------------------------
    void resolvedTargetCounts() {
        QVERIFY(ask(Platform::MacOS, QStringLiteral("/Users/a/liesmich.txt"), false, false, QString(),
                    QStringLiteral("/Users/a/versteckt/werkzeug.command")));
        QVERIFY(ask(Platform::Windows, QStringLiteral("C:/a/liesmich.txt"), false, false, QString(),
                    QStringLiteral("C:/a/versteckt/werkzeug.bat")));
        QVERIFY(ask(Platform::Linux, QStringLiteral("/home/a/liesmich.txt"), false, false, QString(),
                    QStringLiteral("/home/a/starter.desktop")));
        // Ein harmloses Ziel macht nichts verdächtig, und derselbe Pfad zählt nicht doppelt.
        QVERIFY(!ask(Platform::MacOS, QStringLiteral("/Users/a/liesmich.txt"), false, false, QString(),
                     QStringLiteral("/Users/a/andere.txt")));
        QVERIFY(!ask(Platform::Windows, QStringLiteral("C:/a/liesmich.txt"), false, false, QString(),
                     QStringLiteral("C:/a/liesmich.txt")));
        // Umgekehrt: Der Name allein genügt auch.
        QVERIFY(ask(Platform::MacOS, QStringLiteral("/Users/a/werkzeug.command"), false, false, QString(),
                    QStringLiteral("/Users/a/andere.txt")));
    }

    // --- Angaben echter Dateien ---------------------------------------------------------------
    void factsOfRealFiles() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QDir d(dir.path());
        const QString plain = d.absoluteFilePath(QStringLiteral("notiz.txt"));
        {
            QFile f(plain);
            QVERIFY(f.open(QIODevice::WriteOnly));
        }
        QVERIFY(d.mkdir(QStringLiteral("ordner")));

        const Facts missing = factsFor(d.absoluteFilePath(QStringLiteral("gibt-es-nicht")));
        QVERIFY(!missing.isDir && !missing.anyExecBit && missing.resolved.isEmpty());

        const Facts folder = factsFor(d.absoluteFilePath(QStringLiteral("ordner")));
        QVERIFY(folder.isDir);

        const Facts file = factsFor(plain);
        QVERIFY(!file.isDir);
        QCOMPARE(file.resolved, QFileInfo(plain).canonicalFilePath());
#ifndef Q_OS_WIN
        QVERIFY(!file.anyExecBit);
        // Ausführungsrecht nur für andere genügt: im Zweifel fragen.
        QVERIFY(QFile::setPermissions(plain, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOther));
        QVERIFY(factsFor(plain).anyExecBit);
        QVERIFY(QFile::setPermissions(plain, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QVERIFY(factsFor(plain).anyExecBit);
        QVERIFY(QFile::setPermissions(plain, QFile::ReadOwner | QFile::WriteOwner));
        QVERIFY(!factsFor(plain).anyExecBit);
        // Eine symbolische Verknüpfung meldet ihr Ziel.
        const QString link = d.absoluteFilePath(QStringLiteral("liesmich.txt"));
        const QString target = d.absoluteFilePath(QStringLiteral("werkzeug.command"));
        {
            QFile f(target);
            QVERIFY(f.open(QIODevice::WriteOnly));
        }
        QVERIFY(QFile::link(target, link));
        QCOMPARE(factsFor(link).resolved, QFileInfo(target).canonicalFilePath());
#endif
    }

    // --- Die Kurzform auf diesem Rechner ----------------------------------------------------
    void thisMachine() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QDir d(dir.path());
#ifdef Q_OS_WIN
        const QString runs = d.absoluteFilePath(QStringLiteral("werkzeug.bat"));
#else
        const QString runs = d.absoluteFilePath(QStringLiteral("werkzeug"));
#endif
        const QString plain = d.absoluteFilePath(QStringLiteral("notiz.txt"));
        for (const QString &p : {runs, plain}) {
            QFile f(p);
            QVERIFY(f.open(QIODevice::WriteOnly));
        }
#ifndef Q_OS_WIN
        QVERIFY(QFile::setPermissions(runs, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
#endif
        QVERIFY(d.mkdir(QStringLiteral("ordner")));
        QVERIFY(asksBeforeOpen(runs));
        QVERIFY(!asksBeforeOpen(plain));
        QVERIFY(!asksBeforeOpen(d.absoluteFilePath(QStringLiteral("ordner"))));
        QVERIFY(!asksBeforeOpen(d.absoluteFilePath(QStringLiteral("gibt-es-nicht.txt"))));
#ifndef Q_OS_WIN
        // Hinter einem harmlosen Namen: die Verknüpfung auf das Programm fragt ebenfalls.
        const QString link = d.absoluteFilePath(QStringLiteral("liesmich.txt"));
        QVERIFY(QFile::link(runs, link));
        QVERIFY(asksBeforeOpen(link));
#endif
    }
};

QTEST_GUILESS_MAIN(TestExecutableTarget)
#include "tst_executabletarget.moc"

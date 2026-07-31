#include <QtTest>
#include <QCoreApplication>
#include <QFile>
#include <QTranslator>

// QTMUX-117: Die Beschriftungen der Standardknoepfe ("OK", "Cancel", "Save", …) kommen
// NICHT aus unseren .ts, sondern aus Qts eigener Uebersetzung im Kontext QPlatformTheme.
// Ohne einen zweiten, dafuer geladenen Translator heisst der Abbrechen-Knopf jedes
// AppDialog mit `standardButtons` auch auf Deutsch "Cancel".
//
// Dieser Test faehrt exakt die Kette aus main.cpp nach: dieselbe Dateiliste, aus dem
// Ressourcensystem unter :/i18n geladen (die Einbettung uebernimmt dieselbe
// CMake-Logik wie fuer die App). Er faellt damit in beiden Fehlerfaellen — wenn die
// .qm gar nicht erst eingebettet wird, und wenn Qt den Kontext aendert.
class TestI18n : public QObject {
    Q_OBJECT
private slots:
    void qtBaseTranslationsAreEmbedded();
    // Reihenfolge bewusst: die Gegenprobe laeuft VOR dem Installieren des
    // Translators — sonst haenge sie davon ab, dass der Test davor sauber aufraeumt.
    void withoutTranslatorLabelsStayEnglish();
    void standardButtonsAreTranslated();
    void unknownLanguageLeavesSourceText();
};

// Die .qm muss im Ressourcensystem liegen — sonst nuetzt der beste Translator nichts.
// Genau das ging bisher schief: mitgeliefert wurde sie von keinem der drei
// Deployment-Werkzeuge zuverlaessig.
void TestI18n::qtBaseTranslationsAreEmbedded() {
    QVERIFY2(QFile::exists(QStringLiteral(":/i18n/qtbase_de.qm")),
             "qtbase_de.qm fehlt im Ressourcensystem");
}

void TestI18n::standardButtonsAreTranslated() {
    QTranslator tr;
    QVERIFY(tr.load(QStringLiteral("qtbase_de"), QStringLiteral(":/i18n")));
    QVERIFY(QCoreApplication::installTranslator(&tr));

    // Der Kontext ist QPlatformTheme — ueber ihn holt Qt Quick Controls die
    // Standardbeschriftungen. (Am .qm-Inhalt geprueft, 2026-07-31.)
    QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Cancel"),
             QStringLiteral("Abbrechen"));
    QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Save"),
             QStringLiteral("Speichern"));
    QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Close"),
             QStringLiteral("Schließen"));
    // "OK" ist im Deutschen identisch — als Erinnerung, dass ein unveraenderter
    // Text hier KEIN Hinweis auf einen fehlenden Translator ist.
    QCOMPARE(QCoreApplication::translate("QPlatformTheme", "OK"), QStringLiteral("OK"));

    QVERIFY(QCoreApplication::removeTranslator(&tr));
}

// Gegenprobe: ohne installierten Translator bleibt der Quelltext stehen. Faellt dieser
// Fall nicht, misst der Test oben etwas anderes als die Uebersetzung.
void TestI18n::withoutTranslatorLabelsStayEnglish() {
    QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Cancel"),
             QStringLiteral("Cancel"));
}

// Englisch ist Qts Quellsprache; qtbase_en.qm existiert, uebersetzt aber nicht weg.
// Eine unbekannte Sprache darf NICHT laden — sonst bliebe beim Wechsel die zuletzt
// geladene Uebersetzung aktiv (deshalb entfernt swapTranslator sie in main.cpp).
void TestI18n::unknownLanguageLeavesSourceText() {
    QTranslator tr;
    QVERIFY(!tr.load(QStringLiteral("qtbase_xx"), QStringLiteral(":/i18n")));
    QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Cancel"),
             QStringLiteral("Cancel"));
}

QTEST_MAIN(TestI18n)
#include "tst_i18n.moc"

#include <QtTest>
#include <QSettings>
#include "SessionModel.h"
#include "Session.h"

using namespace qtmux;

// Sitzungsgruppen (QTMUX-42): Die Sidebar zeigt Gruppen über ListView-Sections,
// und die setzen zusammenhängende Blöcke voraus. Diese Blockbildung ist deshalb
// Aufgabe des Models — hier getestet, weil sie in der GUI nur mühsam zu prüfen
// und leicht zu zerstören ist (jedes Zuordnen und Ziehen greift ein).
class TestSessionGroups : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void groupsFormContiguousBlocks();
    void firstMemberStaysInPlace();
    void newGroupLeavesForeignBlockIntact();
    void dragAdoptsNeighbourGroup();
    void renameMergesAndDissolves();
    void groupSurvivesSaveAndRestore();

private:
    /// Gruppen aller Zeilen in Sidebar-Reihenfolge — das ist die Größe, auf die
    /// es ankommt (Reihenfolge + Zuordnung in einem).
    static QStringList layout(const SessionModel &m) {
        QStringList out;
        for (const Session *s : m.sessions()) out << s->group();
        return out;
    }
    /// Alle Titel/IDs in Reihenfolge, um Verschiebungen nachzuweisen.
    static QList<int> ids(const SessionModel &m) {
        QList<int> out;
        for (const Session *s : m.sessions()) out << s->id();
        return out;
    }
};

void TestSessionGroups::initTestCase() {
    // Eigene Settings-Domain: der Persistenztest darf keine echte Sitzungsliste
    // überschreiben (die App speichert unter "QTmux").
    QCoreApplication::setOrganizationName(QStringLiteral("QTmux"));
    QCoreApplication::setApplicationName(QStringLiteral("QTmuxGroupsTest"));
}

void TestSessionGroups::init() {
    QSettings().clear();
}

// Vier Sessions, zwei davon in dieselbe Gruppe: die Gruppe muss danach ein
// zusammenhängender Block sein, egal wie verstreut die Zeilen vorher lagen.
void TestSessionGroups::groupsFormContiguousBlocks() {
    SessionModel m;
    for (int i = 0; i < 4; ++i) QVERIFY(m.createShellSession() >= 0);
    const QList<int> before = ids(m);

    m.setSessionGroup(0, QStringLiteral("Release"));
    m.setSessionGroup(2, QStringLiteral("Release"));

    const QStringList l = layout(m);
    QCOMPARE(l.count(QStringLiteral("Release")), 2);
    // Beide Mitglieder stehen nebeneinander.
    QCOMPARE(l.indexOf(QStringLiteral("Release")) + 1,
             l.lastIndexOf(QStringLiteral("Release")));
    // Und in der Reihenfolge ihrer Zuordnung (erst 0, dann die verschobene).
    QCOMPARE(ids(m).at(l.indexOf(QStringLiteral("Release"))), before.at(0));
    QCOMPARE(m.groupSize(QStringLiteral("Release")), 2);
    QCOMPARE(m.groups(), QStringList{QStringLiteral("Release")});
}

// Die erste Zuordnung darf die Kachel nicht wegspringen lassen: solange sie
// keinen fremden Block zerreißt, bleibt sie stehen, wo der Nutzer sie sieht.
void TestSessionGroups::firstMemberStaysInPlace() {
    SessionModel m;
    for (int i = 0; i < 3; ++i) QVERIFY(m.createShellSession() >= 0);
    const QList<int> before = ids(m);

    m.setSessionGroup(1, QStringLiteral("Doku"));

    QCOMPARE(ids(m), before);
    QCOMPARE(layout(m), (QStringList{QString(), QStringLiteral("Doku"), QString()}));
}

// Liegt die Zeile mitten in einem fremden Block, muss sie ihn beim Umgruppieren
// verlassen — sonst erschiene die fremde Gruppe in der Sidebar zweimal.
void TestSessionGroups::newGroupLeavesForeignBlockIntact() {
    SessionModel m;
    for (int i = 0; i < 4; ++i) QVERIFY(m.createShellSession() >= 0);
    for (int i = 0; i < 3; ++i) m.setSessionGroup(i, QStringLiteral("A"));
    QCOMPARE(layout(m).mid(0, 3),
             (QStringList{QStringLiteral("A"), QStringLiteral("A"), QStringLiteral("A")}));

    m.setSessionGroup(1, QStringLiteral("B"));   // mitten aus dem A-Block heraus

    const QStringList l = layout(m);
    QCOMPARE(l.count(QStringLiteral("A")), 2);
    QCOMPARE(l.indexOf(QStringLiteral("A")) + 1, l.lastIndexOf(QStringLiteral("A")));
    QCOMPARE(l.count(QStringLiteral("B")), 1);
}

// Ziehen in einen Gruppenblock nimmt die Session in die Gruppe auf — das ist die
// einzige Stelle, an der eine Bewegung die Zuordnung ändern darf.
void TestSessionGroups::dragAdoptsNeighbourGroup() {
    SessionModel m;
    for (int i = 0; i < 3; ++i) QVERIFY(m.createShellSession() >= 0);
    m.setSessionGroup(0, QStringLiteral("Team"));
    m.setSessionGroup(1, QStringLiteral("Team"));
    QCOMPARE(layout(m).at(2), QString());

    m.moveSession(2, 1);   // ungruppierte Kachel mitten in den Team-Block ziehen

    QCOMPARE(m.groupSize(QStringLiteral("Team")), 3);
    QCOMPARE(layout(m), (QStringList{QStringLiteral("Team"), QStringLiteral("Team"),
                                     QStringLiteral("Team")}));

    // Und wieder heraus: ans Ende gezogen, wo niemand mehr eine Gruppe hat …
    m.setSessionGroup(2, QString());
    QCOMPARE(m.groupSize(QStringLiteral("Team")), 2);
}

// Umbenennen zieht alle Mitglieder mit; auf einen bereits vergebenen Namen
// verschmelzen die Blöcke, ein leerer Name löst die Gruppe auf.
void TestSessionGroups::renameMergesAndDissolves() {
    SessionModel m;
    for (int i = 0; i < 4; ++i) QVERIFY(m.createShellSession() >= 0);
    m.setSessionGroup(0, QStringLiteral("Alt"));
    m.setSessionGroup(1, QStringLiteral("Alt"));
    m.setSessionGroup(2, QStringLiteral("Andere"));

    m.renameGroup(QStringLiteral("Alt"), QStringLiteral("Neu"));
    QCOMPARE(m.groupSize(QStringLiteral("Neu")), 2);
    QCOMPARE(m.groupSize(QStringLiteral("Alt")), 0);

    m.renameGroup(QStringLiteral("Andere"), QStringLiteral("Neu"));   // verschmelzen
    QCOMPARE(m.groupSize(QStringLiteral("Neu")), 3);
    const QStringList l = layout(m);
    QCOMPARE(l.indexOf(QStringLiteral("Neu")) + 2, l.lastIndexOf(QStringLiteral("Neu")));

    m.renameGroup(QStringLiteral("Neu"), QString());                  // auflösen
    QVERIFY(m.groups().isEmpty());
}

// Die Zuordnung muss einen Neustart überleben — sie ist Teil der Sitzungsliste.
void TestSessionGroups::groupSurvivesSaveAndRestore() {
    {
        SessionModel m;
        for (int i = 0; i < 3; ++i) QVERIFY(m.createShellSession() >= 0);
        m.setSessionGroup(0, QStringLiteral("Release 1.5"));
        m.setSessionGroup(1, QStringLiteral("Release 1.5"));
        m.saveState();
    }
    SessionModel restored;
    QCOMPARE(restored.restoreState() >= -1, true);
    QCOMPARE(restored.count(), 3);
    QCOMPARE(restored.groupSize(QStringLiteral("Release 1.5")), 2);
    // Blockbildung bleibt erhalten, ohne dass beim Restore umsortiert wird.
    const QStringList l = layout(restored);
    QCOMPARE(l.at(0), QStringLiteral("Release 1.5"));
    QCOMPARE(l.at(1), QStringLiteral("Release 1.5"));
    QCOMPARE(l.at(2), QString());
}

QTEST_MAIN(TestSessionGroups)
#include "tst_sessiongroups.moc"

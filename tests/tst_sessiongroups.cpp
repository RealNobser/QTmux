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
    void statusBarCounters();
    void groupsFormContiguousBlocks();
    void firstMemberStaysInPlace();
    void newGroupLeavesForeignBlockIntact();
    void dragAdoptsNeighbourGroup();
    void removeFromGroupNotifiesFinalRow();
    void moveGroupReordersBlock();
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

// Aggregat-Zähler der Statusleiste (Design 1a, Teil B): wie viele Sessions warten auf
// Eingabe bzw. melden einen Fehler. Gezählt wird die selbst gemeldete Activity — der
// Startwert Running darf NICHT als „wartet" durchgehen (Gegenprobe unten).
void TestSessionGroups::statusBarCounters() {
    SessionModel m;
    for (int i = 0; i < 3; ++i) QVERIFY(m.createShellSession() >= 0);
    QCOMPARE(m.count(), 3);
    // Frisch angelegt: niemand wartet, niemand meldet Fehler.
    QCOMPARE(m.waitingCount(), 0);
    QCOMPARE(m.errorCount(), 0);

    auto *s0 = qobject_cast<Session *>(m.sessionAt(0));
    auto *s1 = qobject_cast<Session *>(m.sessionAt(1));
    auto *s2 = qobject_cast<Session *>(m.sessionAt(2));
    QVERIFY(s0 && s1 && s2);

    QSignalSpy spy(&m, &SessionModel::countersChanged);
    s0->requestActivity(QStringLiteral("waiting"));
    s1->requestActivity(QStringLiteral("waiting"));
    s2->requestActivity(QStringLiteral("error"));
    QCOMPARE(m.waitingCount(), 2);
    QCOMPARE(m.errorCount(), 1);
    QVERIFY(spy.count() >= 3);      // die Anzeige wird über countersChanged angestoßen

    // Zurück auf „arbeitet": beide Zähler müssen wieder fallen — sonst bliebe in der
    // Statusleiste für immer „1 Fehler" stehen.
    s2->requestActivity(QStringLiteral("busy"));
    QCOMPARE(m.errorCount(), 0);
    QCOMPARE(m.waitingCount(), 2);

    // Gegenprobe: Schließen einer wartenden Session senkt den Zähler.
    m.closeSession(0);
    QCOMPARE(m.waitingCount(), 1);
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

// Bug-Regression: Wird eine Session aus einer Gruppe genommen und dabei umsortiert,
// muss die dataChanged-Benachrichtigung am ENDGÜLTIGEN Index (nach dem Move) kommen.
// Andernfalls (dataChanged VOR dem beginMoveRows) cachte der ListView-Delegate beim
// direkt folgenden Move den alten Gruppenwert wieder ein → Farbmarke/Einrückung blieben
// sporadisch stehen, obwohl das Model bereits stimmte.
void TestSessionGroups::removeFromGroupNotifiesFinalRow() {
    SessionModel m;
    for (int i = 0; i < 3; ++i) QVERIFY(m.createShellSession() >= 0);
    m.setSessionGroup(0, QStringLiteral("X"));
    m.setSessionGroup(1, QStringLiteral("X"));   // Layout: X X ""
    const int idB = m.sessions().at(1)->id();

    const int groupRole = m.roleNames().key("group", -1);
    QVERIFY(groupRole >= 0);

    QSignalSpy spy(&m, &QAbstractItemModel::dataChanged);
    m.setSessionGroup(1, QString());             // B aus Gruppe X nehmen → wird verschoben

    // B ist jetzt ungruppiert und aus dem X-Block heraus (X bleibt zusammenhängend).
    QCOMPARE(m.groupSize(QStringLiteral("X")), 1);
    int finalRowB = -1;
    for (int i = 0; i < m.count(); ++i)
        if (m.sessions().at(i)->id() == idB) { finalRowB = i; break; }
    QVERIFY(finalRowB >= 0);
    QVERIFY(m.sessions().at(finalRowB)->group().isEmpty());

    // Das LETZTE dataChanged mit der group-Rolle muss auf B's endgültige Zeile zeigen.
    int lastRow = -1;
    for (const QList<QVariant> &args : spy) {
        const QList<int> roles = args.at(2).value<QList<int>>();
        if (roles.contains(groupRole))
            lastRow = args.at(0).value<QModelIndex>().row();
    }
    QCOMPARE(lastRow, finalRowB);
}

// moveGroup verschiebt eine ganze Gruppe als Block über die benachbarte Sektion; die
// Mitglieder bleiben zusammenhängend und identisch, nur die Blockreihenfolge ändert sich.
void TestSessionGroups::moveGroupReordersBlock() {
    SessionModel m;
    for (int i = 0; i < 4; ++i) QVERIFY(m.createShellSession() >= 0);
    m.setSessionGroup(0, QStringLiteral("A"));
    m.setSessionGroup(1, QStringLiteral("A"));
    m.setSessionGroup(2, QStringLiteral("B"));
    m.setSessionGroup(3, QStringLiteral("B"));
    const QStringList AABB{"A","A","B","B"};
    QCOMPARE(layout(m), AABB);
    // IDs der A-Mitglieder merken (müssen erhalten bleiben, nur die Position ändert sich).
    QList<int> aIds;
    for (int i = 0; i < m.count(); ++i)
        if (m.sessions().at(i)->group() == QLatin1String("A")) aIds << m.sessions().at(i)->id();

    m.moveGroup(QStringLiteral("A"), 1);          // A nach unten → über den B-Block
    QCOMPARE(layout(m), (QStringList{"B","B","A","A"}));
    QList<int> aIdsAfter;
    for (int i = 0; i < m.count(); ++i)
        if (m.sessions().at(i)->group() == QLatin1String("A")) aIdsAfter << m.sessions().at(i)->id();
    QCOMPARE(aIdsAfter, aIds);                     // dieselben Mitglieder, nur verschoben

    m.moveGroup(QStringLiteral("A"), -1);          // wieder hoch → Ausgangslage
    QCOMPARE(layout(m), AABB);

    m.moveGroup(QStringLiteral("A"), -1);          // schon ganz oben → No-op
    QCOMPARE(layout(m), AABB);

    m.moveGroup(QStringLiteral("Gibtsnicht"), 1);  // unbekannte Gruppe → No-op
    QCOMPARE(layout(m), AABB);

    // Direkter moveGroupToRow (Header-Drag-Pfad): A auf eine Zeile im B-Block fallen
    // lassen → A landet hinter B.
    m.moveGroupToRow(QStringLiteral("A"), 3);
    QCOMPARE(layout(m), (QStringList{"B","B","A","A"}));
    // Auf die eigene Gruppe fallen lassen → No-op.
    m.moveGroupToRow(QStringLiteral("A"), 2);
    QCOMPARE(layout(m), (QStringList{"B","B","A","A"}));
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

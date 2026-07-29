#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTemporaryDir>

#include "Window.h"
#include "WindowModel.h"

using namespace qtmux;

// Per-Window-Layouts (QTMUX-83, Modul A): Das WindowModel ist künftig die Sidebar,
// und jedes Window trägt seinen eigenen Layout-Baum. Getestet wird hier die Kern-
// Invariante der Datenschicht: stabile IDs (auch über das Schließen hinweg), die
// Ableitung der Panes AUS dem Baum (single source of truth = layoutJson) und die
// Nachführung der aktiven Zeile — alles Dinge, die in der GUI nur mühsam zu prüfen sind.
class TestWindowModel : public QObject {
    Q_OBJECT
private slots:
    void createAndCloseWindow();
    void lookupByIdAndRow();
    void paneCountFollowsLayout();
    void sessionIdsInLeafOrder();
    void activeRowFollowsClose();
    void windowIdsStayUniqueAfterSetNextId();
    void migrateOldSessionsToWindows();
    void groupsStayContiguous();
    void restoreModeGatesLayoutHistoryAndPersistence();
    void unknownRestoreModeFallsBackToFull();

private:
    /// Ein Split-Baum mit zwei Blättern (Format wie im QML-Layout-Baum).
    static QString twoPaneLayout(int paneA, int sessA, int paneB, int sessB) {
        return QStringLiteral(
                   R"({"orientation":1,"sizes":[0.5,0.5],"children":[)"
                   R"({"paneId":%1,"sessionId":%2,"cfg":{"type":0}},)"
                   R"({"paneId":%3,"sessionId":%4,"cfg":{"type":0}}]})")
            .arg(paneA)
            .arg(sessA)
            .arg(paneB)
            .arg(sessB);
    }
    static QString onePaneLayout(int paneId, int sessionId) {
        return QStringLiteral(R"({"paneId":%1,"sessionId":%2,"cfg":{"type":0}})")
            .arg(paneId)
            .arg(sessionId);
    }
};

// Anlegen liefert die Zeile, zählt hoch und meldet es; Schließen entfernt genau eines.
void TestWindowModel::createAndCloseWindow() {
    WindowModel m;
    QCOMPARE(m.count(), 0);
    QSignalSpy countSpy(&m, &WindowModel::countChanged);

    QCOMPARE(m.createWindow(), 0);
    QCOMPARE(m.createWindow(QStringLiteral("Build")), 1);
    QCOMPARE(m.count(), 2);
    QCOMPARE(countSpy.count(), 2);
    QCOMPARE(m.rowCount(), 2);

    auto *w1 = qobject_cast<Window *>(m.windowAt(1));
    QVERIFY(w1);
    QCOMPARE(w1->name(), QStringLiteral("Build"));
    // Ohne Namen springt der Titel auf einen automatischen ein (nie leer).
    const int titleRole = m.roleNames().key("title", -1);
    QVERIFY(titleRole >= 0);
    QVERIFY(!m.data(m.index(0), titleRole).toString().isEmpty());
    QCOMPARE(m.data(m.index(1), titleRole).toString(), QStringLiteral("Build"));

    const int id0 = qobject_cast<Window *>(m.windowAt(0))->id();
    m.closeWindow(0);
    QCOMPARE(m.count(), 1);
    QCOMPARE(m.rowForId(id0), -1);
    QVERIFY(!m.windowById(id0));
    // Das verbliebene Window ist unverändert dasselbe Objekt.
    QCOMPARE(qobject_cast<Window *>(m.windowAt(0))->name(), QStringLiteral("Build"));

    m.closeWindow(5);   // außerhalb → No-op
    m.closeWindow(-1);
    QCOMPARE(m.count(), 1);
}

// Die Window-ID ist der stabile Anker (wie Session::id); Zeilen sind es nicht.
void TestWindowModel::lookupByIdAndRow() {
    WindowModel m;
    m.createWindow(QStringLiteral("A"));
    m.createWindow(QStringLiteral("B"));
    m.createWindow(QStringLiteral("C"));

    auto *a = qobject_cast<Window *>(m.windowAt(0));
    auto *b = qobject_cast<Window *>(m.windowAt(1));
    auto *c = qobject_cast<Window *>(m.windowAt(2));
    QVERIFY(a && b && c);
    // IDs sind paarweise verschieden und monoton.
    QVERIFY(a->id() < b->id());
    QVERIFY(b->id() < c->id());

    QCOMPARE(m.windowById(b->id()), b);
    QCOMPARE(m.rowForId(b->id()), 1);
    QCOMPARE(m.rowForId(c->id()), 2);

    // Nach dem Schließen von A rutscht C hoch — die ID bleibt derselbe Anker.
    m.closeWindow(0);
    QCOMPARE(m.rowForId(c->id()), 1);
    QCOMPARE(m.windowById(c->id()), c);
    QCOMPARE(m.rowForId(9999), -1);
    QVERIFY(!m.windowById(9999));
    QVERIFY(!m.windowAt(7));
}

// paneCount wird AUS dem Baum abgeleitet — es gibt keine zweite, driftende Quelle.
void TestWindowModel::paneCountFollowsLayout() {
    WindowModel m;
    m.createWindow();
    auto *w = qobject_cast<Window *>(m.windowAt(0));
    QVERIFY(w);

    const int paneRole = m.roleNames().key("paneCount", -1);
    QVERIFY(paneRole >= 0);
    // Frisches Window: leerer Baum = keine Panes (kein Absturz an leerem/kaputtem JSON).
    QCOMPARE(m.data(m.index(0), paneRole).toInt(), 0);
    w->setLayoutJson(QStringLiteral("null"));
    QCOMPARE(w->paneCount(), 0);
    w->setLayoutJson(QStringLiteral("{kaputt"));
    QCOMPARE(w->paneCount(), 0);

    QSignalSpy changed(&m, &QAbstractItemModel::dataChanged);
    w->setLayoutJson(twoPaneLayout(1, 11, 2, 12));

    QCOMPARE(w->paneCount(), 2);
    QCOMPARE(m.data(m.index(0), paneRole).toInt(), 2);
    // Der Baum ist die Authority: seine Änderung muss die Zeile melden, sonst friert
    // die Sidebar auf dem alten Stand ein.
    QVERIFY(changed.count() >= 1);
    QCOMPARE(changed.last().at(0).value<QModelIndex>().row(), 0);
    QVERIFY(changed.last().at(2).value<QList<int>>().contains(paneRole));

    w->setLayoutJson(onePaneLayout(1, 11));
    QCOMPARE(m.data(m.index(0), paneRole).toInt(), 1);

    // TODO-Stubs (Modul B): neutral, bis die Aggregation über die Sessions steht.
    QCOMPARE(m.data(m.index(0), m.roleNames().key("runState", -1)).toInt(), 0);
    QCOMPARE(m.data(m.index(0), m.roleNames().key("needsAttention", -1)).toBool(), false);
    QCOMPARE(m.data(m.index(0), m.roleNames().key("mcpController", -1)).toBool(), false);
}

// Die Panes eines Windows sind seine Blätter — in Baum-Reihenfolge, verschachtelt wie flach.
void TestWindowModel::sessionIdsInLeafOrder() {
    Window w(Window::nextId());
    QVERIFY(w.sessionIds().isEmpty());

    w.setLayoutJson(onePaneLayout(3, 42));
    QCOMPARE(w.sessionIds(), QList<int>{42});
    QCOMPARE(w.paneIds(), QList<int>{3});

    // Verschachtelt: linkes Blatt, rechts ein Unter-Split mit zwei Blättern.
    w.setLayoutJson(QStringLiteral(
        R"({"orientation":1,"children":[)"
        R"({"paneId":1,"sessionId":10},)"
        R"({"orientation":2,"children":[{"paneId":2,"sessionId":20},)"
        R"({"paneId":3,"sessionId":30}]}]})"));
    QCOMPARE(w.sessionIds(), (QList<int>{10, 20, 30}));
    QCOMPARE(w.paneIds(), (QList<int>{1, 2, 3}));
    QCOMPARE(w.paneCount(), 3);

    QSignalSpy nameSpy(&w, &Window::nameChanged);
    QSignalSpy groupSpy(&w, &Window::groupChanged);
    w.setName(QStringLiteral("Release"));
    w.setName(QStringLiteral("Release"));   // gleicher Wert → kein zweites Signal
    w.setGroup(QStringLiteral("Team"));
    QCOMPARE(nameSpy.count(), 1);
    QCOMPARE(groupSpy.count(), 1);
    w.setActivePaneId(2);
    QCOMPARE(w.activePaneId(), 2);
}

// Beim Schließen darf die Auswahl weder ins Leere zeigen noch auf ein fremdes Window springen.
void TestWindowModel::activeRowFollowsClose() {
    WindowModel m;
    for (int i = 0; i < 3; ++i) m.createWindow();
    auto *last = qobject_cast<Window *>(m.windowAt(2));
    QVERIFY(last);
    last->setLayoutJson(onePaneLayout(7, 77));

    m.setActiveRow(2);
    QCOMPARE(m.activeRow(), 2);
    m.setActiveRow(9);          // außerhalb → ignoriert
    QCOMPARE(m.activeRow(), 2);

    QSignalSpy closedSpy(&m, &WindowModel::windowClosed);
    m.closeWindow(0);           // Zeile darüber weg → Auswahl rutscht mit
    QCOMPARE(m.activeRow(), 1);
    QCOMPARE(closedSpy.count(), 1);
    QCOMPARE(closedSpy.at(0).at(1).value<QList<int>>(), QList<int>{});

    m.closeWindow(1);           // die aktive Zeile selbst → letzte verbleibende
    QCOMPARE(m.activeRow(), 0);
    // Das Schließen meldet die Sessions der Panes, damit der Besitzer sie beenden kann.
    QCOMPARE(closedSpy.at(1).at(1).value<QList<int>>(), QList<int>{77});

    m.closeWindow(0);           // nichts mehr übrig
    QCOMPARE(m.count(), 0);
    QCOMPARE(m.activeRow(), -1);
}

// Anders als Session::id muss die Window-ID Neustarts überleben: der Restore schreibt
// den Zähler auf den höchsten geladenen Wert fort, sonst vergäbe das nächste
// createWindow() eine bereits belegte ID.
void TestWindowModel::windowIdsStayUniqueAfterSetNextId() {
    WindowModel m;
    Window::setNextId(5000);
    Window restored(5000);      // „geladenes" Window mit hoher ID
    QCOMPARE(restored.id(), 5000);

    m.createWindow();
    auto *fresh = qobject_cast<Window *>(m.windowAt(0));
    QVERIFY(fresh);
    QVERIFY(fresh->id() > 5000);

    // Ein noch höheres restauriertes Window zieht den Zähler ebenfalls mit (der
    // Konstruktor selbst schreibt fort) — und zurück geht er nie.
    Window high(9000);
    Window::setNextId(10);      // kleinerer Wert → wirkungslos
    m.createWindow();
    auto *next = qobject_cast<Window *>(m.windowAt(1));
    QVERIFY(next);
    QVERIFY(next->id() > high.id());
}

// Migration (QTMUX-83): das alte flache `sessions`-Array wird zu einem `windows`-Array,
// je Session ein Ein-Pane-Window — Gruppe erhalten, Blatt trägt den SessionConfig, aktive
// Zeile übernommen, idempotent. Reiner QSettings-Transform, hier gegen eine Temp-INI geprüft.
void TestWindowModel::migrateOldSessionsToWindows() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("t.ini"));
    {   // Altes Schema seeden: Shell(+Gruppe), SSH, Seriell.
        QSettings s(path, QSettings::IniFormat);
        s.beginWriteArray(QStringLiteral("sessions"), 3);
        s.setArrayIndex(0);
        s.setValue(QStringLiteral("type"), 0);
        s.setValue(QStringLiteral("program"), QStringLiteral("/bin/zsh"));
        s.setValue(QStringLiteral("workingDir"), QStringLiteral("/tmp"));
        s.setValue(QStringLiteral("group"), QStringLiteral("Alpha"));
        s.setArrayIndex(1);
        s.setValue(QStringLiteral("type"), 1);
        s.setValue(QStringLiteral("host"), QStringLiteral("example.com"));
        s.setValue(QStringLiteral("sshPort"), 2222);
        s.setValue(QStringLiteral("user"), QStringLiteral("nob"));
        s.setArrayIndex(2);
        s.setValue(QStringLiteral("type"), 2);
        s.setValue(QStringLiteral("serialPort"), QStringLiteral("/dev/tty"));
        s.setValue(QStringLiteral("baud"), 9600);
        s.endArray();
        s.setValue(QStringLiteral("sessions/activeRow"), 1);
        s.sync();
    }
    {   // Migrieren.
        QSettings s(path, QSettings::IniFormat);
        WindowModel::migrateSessionsToWindows(s);
        s.sync();
    }

    QSettings s(path, QSettings::IniFormat);
    QCOMPARE(s.beginReadArray(QStringLiteral("windows")), 3);

    // Window 1 (SSH): Blatt-cfg farbtreu übernommen, sessionId als Restore-Platzhalter -1.
    s.setArrayIndex(1);
    QCOMPARE(s.value(QStringLiteral("group")).toString(), QString());   // SSH war ohne Gruppe
    const QString lj = s.value(QStringLiteral("layoutJson")).toString();
    s.setArrayIndex(0);
    QCOMPARE(s.value(QStringLiteral("group")).toString(), QStringLiteral("Alpha"));  // Gruppe erhalten
    s.endArray();

    QVERIFY(!lj.isEmpty());
    const QJsonObject leaf = QJsonDocument::fromJson(lj.toUtf8()).object();
    QVERIFY(leaf.contains(QStringLiteral("paneId")));
    QCOMPARE(leaf.value(QStringLiteral("sessionId")).toInt(), -1);
    const QJsonObject cfg = leaf.value(QStringLiteral("cfg")).toObject();
    QCOMPARE(cfg.value(QStringLiteral("type")).toInt(), 1);
    QCOMPARE(cfg.value(QStringLiteral("host")).toString(), QStringLiteral("example.com"));
    QCOMPARE(cfg.value(QStringLiteral("sshPort")).toInt(), 2222);

    QCOMPARE(s.value(QStringLiteral("windows/activeRow")).toInt(), 1);   // aus sessions/activeRow

    // Idempotenz: erneuter Aufruf lässt das neue Schema unangetastet.
    WindowModel::migrateSessionsToWindows(s);
    QCOMPARE(s.beginReadArray(QStringLiteral("windows")), 3);
    s.endArray();
}

// Window-Gruppen (Stufe 5): jede Gruppe bleibt ein zusammenhängender Block (die Sidebar
// zeigt sie über ListView-Sections). setWindowGroup rückt an den Block; renameGroup
// verschmilzt; moveGroup verschiebt als Block — alles ohne Lücken.
void TestWindowModel::groupsStayContiguous() {
    WindowModel m;
    for (int i = 0; i < 4; ++i) m.createWindow(QStringLiteral("w%1").arg(i));   // Zeilen 0..3
    auto grp = [&](int r) { return qobject_cast<Window *>(m.windowAt(r))->group(); };
    auto contiguous = [&](const QString &g) {
        int first = -1, last = -1;
        for (int i = 0; i < m.count(); ++i)
            if (grp(i) == g) { if (first < 0) first = i; last = i; }
        if (first < 0) return true;
        for (int i = first; i <= last; ++i)
            if (grp(i) != g) return false;   // Lücke im Block
        return true;
    };

    // Zeile 0 und 2 in Gruppe A: das zweite Mitglied rückt an das erste → Block.
    m.setWindowGroup(0, QStringLiteral("A"));
    m.setWindowGroup(2, QStringLiteral("A"));
    QVERIFY(contiguous(QStringLiteral("A")));
    QCOMPARE(m.groupSize(QStringLiteral("A")), 2);
    QCOMPARE(m.groups(), (QStringList{QStringLiteral("A")}));

    // Ein drittes Mitglied in einer anderen Gruppe.
    m.setWindowGroup(m.count() - 1, QStringLiteral("B"));
    QVERIFY(contiguous(QStringLiteral("A")));
    QVERIFY(contiguous(QStringLiteral("B")));
    QCOMPARE(m.groups().size(), 2);

    // Reines Umbenennen (kein Merge) hält den Block zusammen und tauscht nur den Namen.
    m.renameGroup(QStringLiteral("A"), QStringLiteral("C"));
    QCOMPARE(m.groupSize(QStringLiteral("A")), 0);
    QCOMPARE(m.groupSize(QStringLiteral("C")), 2);
    QVERIFY(contiguous(QStringLiteral("C")));

    // Ein Mitglied aus der Gruppe nehmen — der Rest bleibt zusammenhängend.
    int cr = -1;
    for (int i = 0; i < m.count(); ++i)
        if (grp(i) == QStringLiteral("C")) { cr = i; break; }
    m.setWindowGroup(cr, QString());
    QCOMPARE(m.groupSize(QStringLiteral("C")), 1);
    QVERIFY(contiguous(QStringLiteral("C")));

    // Auflösen (leerer Zielname) entfernt die Gruppe ganz.
    m.renameGroup(QStringLiteral("B"), QString());
    QCOMPARE(m.groupSize(QStringLiteral("B")), 0);

    // Gruppe als Block verschieben — bleibt zusammenhängend.
    m.setWindowGroup(0, QStringLiteral("C"));   // C wieder auf 2 Mitglieder bringen
    QVERIFY(contiguous(QStringLiteral("C")));
    m.moveGroup(QStringLiteral("C"), 1);
    QVERIFY(contiguous(QStringLiteral("C")));
}

// --- Umfang der Wiederherstellung (QTMUX-99) -------------------------------------
// Drei Entscheidungen hängen an einer Zahl, und eine davon ist datenkritisch: ob beim
// Beenden überhaupt gespeichert werden darf. Deshalb hier festgenagelt statt in QML
// verstreut abgefragt.
void TestWindowModel::restoreModeGatesLayoutHistoryAndPersistence() {
    WindowModel m;

    // 0 = gar nicht: nichts kommt zurück — und es wird auch NICHT gespeichert, sonst
    // überschriebe das erste Beenden den gespeicherten Stand mit der leeren Session.
    QVERIFY(!m.restoresLayout(0));
    QVERIFY(!m.restoresHistory(0));
    QVERIFY(!m.persistsOnQuit(0));

    // 1 = ohne Verlauf: Fenster, Panes und Arbeitsverzeichnisse ja, Scrollback nein.
    // Gespeichert wird normal — nur das Laden des Verlaufs entfällt.
    QVERIFY(m.restoresLayout(1));
    QVERIFY(!m.restoresHistory(1));
    QVERIFY(m.persistsOnQuit(1));

    // 2 = alles (Vorgabe, bisheriges Verhalten).
    QVERIFY(m.restoresLayout(2));
    QVERIFY(m.restoresHistory(2));
    QVERIFY(m.persistsOnQuit(2));
}

// Ein unlesbarer Wert darf NIE als „gar nicht" durchgehen: das unterdrückt zusätzlich
// das Speichern und fröre den letzten Stand stillschweigend ein — für den Anwender
// nicht von einem Datenverlust zu unterscheiden. Sichere Richtung ist „alles".
void TestWindowModel::unknownRestoreModeFallsBackToFull() {
    WindowModel m;
    const int garbage[] = { 3, 99, -1, -7 };
    for (int v : garbage) {
        QVERIFY2(m.restoresLayout(v),  qPrintable(QStringLiteral("Modus %1").arg(v)));
        QVERIFY2(m.restoresHistory(v), qPrintable(QStringLiteral("Modus %1").arg(v)));
        QVERIFY2(m.persistsOnQuit(v),  qPrintable(QStringLiteral("Modus %1").arg(v)));
    }
}

QTEST_MAIN(TestWindowModel)
#include "tst_windowmodel.moc"

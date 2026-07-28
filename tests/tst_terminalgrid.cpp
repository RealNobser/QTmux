#include <QtTest>
#include <limits>        // std::numeric_limits (NaN-Fall) — MSVC zieht es transitiv, GCC/Clang nicht

#include "TerminalGrid.h"

using namespace qtmux;

/// Tests für die Gui-freie Raster-Berechnung (QTMUX-86).
///
/// Kern des Fehlers: Beim Abbau/Neuaufbau des Pane-Baums (Window-Wechsel) meldet Qt für ein
/// Item transient die Größe 0×0. Das alte `recomputeGrid()` klemmte Spalten/Zeilen auf
/// mindestens 1 und rief `Session::resize(1, 1)` — libvterm kürzte dabei jede Zeile auf ihr
/// erstes Zeichen und schob sie in den Scrollback (Belegartefakt im Scrollback: einzelne `H`
/// aus `H:\_ClaudeWorkspace>`). Das Pane sah leer aus, obwohl es korrekt zeichnete.
class TestTerminalGrid : public QObject {
    Q_OBJECT
private slots:
    void normalSizeGivesGrid() {
        const TerminalGrid g = gridFor(800, 600, 8, 16);
        QVERIFY(g.valid);
        QCOMPARE(g.cols, 100);
        QCOMPARE(g.rows, 37);   // 600/16 = 37,5 → abgeschnitten
    }

    void zeroSizeIsInvalid() {
        // ⚠️ Der eigentliche Regressionsfall. Gegentest zur alten Logik: die hätte hier
        // cols/rows = max(0, 1) = 1×1 geliefert und damit resized — genau der Datenverlust.
        for (const TerminalGrid g : { gridFor(0, 600, 8, 16),
                                      gridFor(800, 0, 8, 16),
                                      gridFor(0, 0, 8, 16) }) {
            QVERIFY(!g.valid);
            QCOMPARE(g.cols, 0);   // 0, NICHT auf 1 geklemmt — der Aufrufer resizt nicht
            QCOMPARE(g.rows, 0);
        }
    }

    void negativeAndNanAreInvalid() {
        QVERIFY(!gridFor(-1, 600, 8, 16).valid);
        QVERIFY(!gridFor(800, -1, 8, 16).valid);
        // NaN: `!(x > 0)` fängt es, `x <= 0` würde es durchlassen.
        const qreal nan = std::numeric_limits<qreal>::quiet_NaN();
        QVERIFY(!gridFor(nan, 600, 8, 16).valid);
        QVERIFY(!gridFor(800, nan, 8, 16).valid);
    }

    void invalidCellMetricsAreInvalid() {
        // Zellmaße stehen erst nach der ersten Font-Messung fest.
        QVERIFY(!gridFor(800, 600, 0, 16).valid);
        QVERIFY(!gridFor(800, 600, 8, 0).valid);
    }

    void tinyButValidSizeClampsToOne() {
        // Gültige, nur winzige Größe darf weiterhin ein 1×1-Raster ergeben — ein echtes
        // Pane wird nie so klein (SplitView.minimumWidth 140), aber die Funktion soll
        // nicht 0 zurückgeben und damit ein gültiges Layout blockieren.
        const TerminalGrid g = gridFor(4, 8, 8, 16);
        QVERIFY(g.valid);
        QCOMPARE(g.cols, 1);
        QCOMPARE(g.rows, 1);
    }
};

QTEST_APPLESS_MAIN(TestTerminalGrid)
#include "tst_terminalgrid.moc"

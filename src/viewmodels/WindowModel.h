#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QVariantList>
#include <QVariantMap>
#include <qqmlintegration.h>

QT_FORWARD_DECLARE_CLASS(QSettings)

namespace qtmux {

class Window;

/// Die Sidebar im Per-Window-Layout-Modell (QTMUX-83, Modul A): eine flache Liste
/// von Windows. Jedes Window trägt seinen eigenen Split-Layout-Baum; Splits erzeugen
/// Panes **im** Window und keine neue Sidebar-Kachel.
///
/// Die API spiegelt bewusst die Formen des heutigen `SessionModel`
/// (`count`/`windowAt`/`rowForId`/`activeRow`), damit der QML-Umbau in Stufe 2
/// minimal ausfällt.
///
/// Besitz: Das Model besitzt die `Window`-Objekte (Metadaten + Baum), **nicht** die
/// Sessions — die bleiben beim `SessionModel`. `closeWindow()` meldet die betroffenen
/// Sessions deshalb nur über `windowClosed()`; das Beenden übernimmt der Besitzer.
class WindowModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int activeRow READ activeRow WRITE setActiveRow NOTIFY activeRowChanged)
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        GroupRole,
        PaneCountRole,
        RunStateRole,
        AttentionRole,
        McpControllerRole,
        WindowRole,
    };
    Q_ENUM(Roles)

    explicit WindowModel(QObject *parent = nullptr);
    ~WindowModel() override;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(m_windows.size()); }

    /// Window-Objekt einer Zeile (für Bindungen in QML), oder nullptr.
    Q_INVOKABLE QObject *windowAt(int row) const;
    /// Window-Objekt zu einer stabilen Window-ID, oder nullptr.
    Q_INVOKABLE Window *windowById(int id) const;
    /// Zeilenindex einer Window-ID, oder -1.
    Q_INVOKABLE int rowForId(int id) const;

    int activeRow() const { return m_activeRow; }
    /// Markiert die Zeile als aktiv. -1 = keine. Außerhalb liegende Werte werden ignoriert.
    Q_INVOKABLE void setActiveRow(int row);

    /// Legt ein leeres Window an (für „+" bzw. das künftige MCP-Tool `new_window`)
    /// und gibt dessen Zeile zurück. Panes entstehen erst durch das Layout.
    Q_INVOKABLE int createWindow(const QString &name = {});
    /// Wie createWindow, aber mit einer VORGEGEBENEN (restaurierten) Window-ID —
    /// die ID muss über Neustarts stabil bleiben (der Zähler wird auf `id`
    /// fortgeschrieben). Für den Restore (Stufe 3). name/group werden gesetzt.
    Q_INVOKABLE int createWindowWithId(int id, const QString &name = {}, const QString &group = {});
    /// Entfernt das Window der Zeile. Die zugehörigen Sessions werden **nicht** hier
    /// beendet — sie gehören dem SessionModel; `windowClosed()` meldet sie dem Besitzer.
    Q_INVOKABLE void closeWindow(int row);

    const QList<Window *> &windows() const { return m_windows; }

    // --- Window-Gruppen (QTMUX-83, Stufe 5) — 1:1 von SessionModel portiert -------
    // Die Sidebar zeigt Gruppen über ListView-Sections (zusammenhängende Blöcke), deshalb
    // hält das Model die Blöcke, nicht die View. Persistenz läuft über persistWindows()
    // beim Quit (das `group`-Feld je Window wird mitgeschrieben) — daher hier KEIN
    // eigenes saveState wie bei SessionModel.
    /// Ordnet ein Window (Zeile) einer Gruppe zu (leer = ohne Gruppe) und rückt es ans
    /// Ende seines Gruppenblocks.
    Q_INVOKABLE void setWindowGroup(int row, const QString &name);
    /// Alle vergebenen Gruppennamen in Sidebar-Reihenfolge (ohne Leereintrag).
    Q_INVOKABLE QStringList groups() const;
    /// Zahl der Windows in einer Gruppe (für die Kopfzeile).
    Q_INVOKABLE int groupSize(const QString &name) const;
    /// Benennt eine Gruppe um (alle Mitglieder). Leerer Zielname löst sie auf.
    Q_INVOKABLE void renameGroup(const QString &from, const QString &to);
    /// Verschiebt eine ganze Gruppe als Block (dir < 0 = hoch, > 0 = runter).
    Q_INVOKABLE void moveGroup(const QString &name, int dir);
    /// Verschiebt die Gruppe als Block vor/nach die Sektion, in der `targetRow` liegt.
    Q_INVOKABLE void moveGroupToRow(const QString &name, int targetRow);
    /// Verschiebt ein Window von `from` nach `to` (Drag-Reorder) und übernimmt die Gruppe
    /// der neuen Nachbarschaft.
    Q_INVOKABLE void moveWindow(int from, int to);

    /// Einmalige Migration (QTMUX-83): das alte flache `sessions`-Array (Vor-B1) in das
    /// neue `windows`-Array überführen — je Session ein Ein-Pane-Window (Gruppe erhalten,
    /// das Blatt trägt den SessionConfig als `cfg`). Idempotent: läuft nur, wenn `windows`
    /// fehlt aber `sessions` existiert; das alte Schema bleibt vorerst stehen (der Restore
    /// legt erst in Stufe 3 um). Statischer, reiner QSettings-Transform → unit-testbar ohne
    /// GUI/Sessions.
    static void migrateSessionsToWindows(QSettings &s);
    /// QML-Auslöser für die Migration (Stufe 3): führt migrateSessionsToWindows auf der
    /// Default-QSettings-Domain aus. Idempotent (läuft nur, wenn `windows` fehlt).
    Q_INVOKABLE void runMigration();

    /// Persistiert das Window-Layout (Stufe 3). `wins` = Liste von Maps
    /// {id,name,group,activePaneId,layoutJson}; layoutJson trägt in den Blättern den
    /// SessionConfig als `cfg` (damit die Sessions restaurierbar sind). Schreibt das
    /// `windows`-Array + activeRow + nextPaneId und entfernt das alte `sessions`-Schema.
    Q_INVOKABLE void writeWindows(const QVariantList &wins, int activeRow, int nextPaneId);
    /// Liest das persistierte Window-Layout zurück:
    /// {present:bool, windows:[{id,name,group,activePaneId,layoutJson}], activeRow, nextPaneId}.
    Q_INVOKABLE QVariantMap readWindows() const;

    // --- Umfang der Wiederherstellung (QTMUX-99) ---------------------------------
    // Die Regeln liegen Gui-frei in [RestoreMode.h](../core/RestoreMode.h) und sind dort
    // dokumentiert; hier stehen nur die QML-Brücken. `mode` kommt roh aus den Einstellungen
    // und wird in jeder der drei Funktionen normalisiert — QML muss den Wert also nicht
    // prüfen, und ein defekter Wert fällt einheitlich auf „alles wiederherstellen" zurück.

    /// Sollen Fenster, Panes und Arbeitsverzeichnisse wiederhergestellt werden?
    Q_INVOKABLE bool restoresLayout(int mode) const;
    /// Soll zusätzlich der farbige Scrollback je Pane geladen werden?
    Q_INVOKABLE bool restoresHistory(int mode) const;
    /// Darf der aktuelle Stand beim Beenden gespeichert werden? ⚠️ Bei „gar nicht" NEIN,
    /// sonst überschreibt das erste Beenden den gespeicherten Stand unwiderruflich.
    Q_INVOKABLE bool persistsOnQuit(int mode) const;

signals:
    void countChanged();
    void activeRowChanged();
    /// Eine Gruppenzuordnung/-reihenfolge hat sich geändert (Anker für QML-Bindungen an
    /// groups()/groupSize(), die keine Property-Notify haben).
    void groupsChanged();
    /// Ein Window wurde geschlossen: `sessionIds` sind die Sessions seiner Panes,
    /// die der Besitzer (SessionModel) jetzt beenden muss.
    void windowClosed(int windowId, const QList<int> &sessionIds);

private:
    /// Verdrahtet die Änderungssignale eines Windows auf dataChanged seiner Zeile.
    void wireWindow(Window *w);
    /// Meldet die Zeile eines Windows als geändert (Rollen-selektiv).
    void emitRowChanged(const Window *w, const QList<int> &roles);

    // Gruppen-Helfer (portiert aus SessionModel): Block-/Zeilen-Verschiebung mit
    // Nachführung der aktiven Zeile; regroupRow hält jede Gruppe als zusammenhängenden Block.
    int regroupRow(int row);
    bool moveRowInternal(int from, int to);
    bool moveBlock(int first, int last, int dest);

    QList<Window *> m_windows;
    int m_activeRow = -1;
};

} // namespace qtmux

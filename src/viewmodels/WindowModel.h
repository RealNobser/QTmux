#pragma once

#include <QAbstractListModel>
#include <QList>
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
    Window *windowById(int id) const;
    /// Zeilenindex einer Window-ID, oder -1.
    Q_INVOKABLE int rowForId(int id) const;

    int activeRow() const { return m_activeRow; }
    /// Markiert die Zeile als aktiv. -1 = keine. Außerhalb liegende Werte werden ignoriert.
    Q_INVOKABLE void setActiveRow(int row);

    /// Legt ein leeres Window an (für „+" bzw. das künftige MCP-Tool `new_window`)
    /// und gibt dessen Zeile zurück. Panes entstehen erst durch das Layout.
    Q_INVOKABLE int createWindow(const QString &name = {});
    /// Entfernt das Window der Zeile. Die zugehörigen Sessions werden **nicht** hier
    /// beendet — sie gehören dem SessionModel; `windowClosed()` meldet sie dem Besitzer.
    Q_INVOKABLE void closeWindow(int row);

    const QList<Window *> &windows() const { return m_windows; }

    /// Einmalige Migration (QTMUX-83): das alte flache `sessions`-Array (Vor-B1) in das
    /// neue `windows`-Array überführen — je Session ein Ein-Pane-Window (Gruppe erhalten,
    /// das Blatt trägt den SessionConfig als `cfg`). Idempotent: läuft nur, wenn `windows`
    /// fehlt aber `sessions` existiert; das alte Schema bleibt vorerst stehen (der Restore
    /// legt erst in Stufe 3 um). Statischer, reiner QSettings-Transform → unit-testbar ohne
    /// GUI/Sessions.
    static void migrateSessionsToWindows(QSettings &s);

signals:
    void countChanged();
    void activeRowChanged();
    /// Ein Window wurde geschlossen: `sessionIds` sind die Sessions seiner Panes,
    /// die der Besitzer (SessionModel) jetzt beenden muss.
    void windowClosed(int windowId, const QList<int> &sessionIds);

private:
    /// Verdrahtet die Änderungssignale eines Windows auf dataChanged seiner Zeile.
    void wireWindow(Window *w);
    /// Meldet die Zeile eines Windows als geändert (Rollen-selektiv).
    void emitRowChanged(const Window *w, const QList<int> &roles);

    QList<Window *> m_windows;
    int m_activeRow = -1;
};

} // namespace qtmux

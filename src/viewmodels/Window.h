#pragma once

#include <QList>
#include <QObject>
#include <QString>

namespace qtmux {

/// Ein Sidebar-Eintrag im Per-Window-Layout-Modell (QTMUX-83, Modul A).
///
/// Ein Window bündelt Metadaten (Name, Gruppe) mit **seinem eigenen** Split-Layout-Baum.
/// Der Baum ist die Authority und liegt hier als JSON (`layoutJson`); QML arbeitet mit
/// einer deserialisierten Live-Kopie des AKTIVEN Windows und schreibt bei jeder
/// Strukturänderung zurück. Blätter referenzieren Sessions über die **stabile**
/// `Session::id()` — nicht mehr über einen Zeilenindex, damit Umsortieren in der
/// Sidebar keinen Index-Remap mehr braucht.
///
/// Baum-Format:
///   Blatt: `{ "paneId": int, "sessionId": int, "cfg": { … } }`
///   Split: `{ "orientation": int, "children": [ … ], "sizes": [ … ] }`
///
/// Bewusst **nicht** als QML-Typ registriert (kein `QML_ELEMENT`): der Name „Window"
/// kollidierte sonst mit `QtQuick.Window.Window`. Die Instanzen erreicht QML — wie bei
/// `Session` — als `QObject *` über das Model.
class Window : public QObject {
    Q_OBJECT
    Q_PROPERTY(int windowId READ id CONSTANT)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString group READ group WRITE setGroup NOTIFY groupChanged)
    Q_PROPERTY(QString layoutJson READ layoutJson WRITE setLayoutJson NOTIFY layoutChanged)
    Q_PROPERTY(int activePaneId READ activePaneId WRITE setActivePaneId NOTIFY activePaneChanged)
public:
    explicit Window(int id, QObject *parent = nullptr);

    /// Persistierte, über Neustarts stabile Window-ID.
    int id() const { return m_id; }

    /// Frei wählbarer Name; leer = automatisch (Titel des aktiven Panes).
    QString name() const { return m_name; }
    void setName(const QString &name);

    /// Window-Gruppe (Stufe 5), leer = ohne Gruppe.
    QString group() const { return m_group; }
    void setGroup(const QString &group);

    /// Der Split-Layout-Baum als JSON. Leer/„null" = leeres Window (keine Panes).
    QString layoutJson() const { return m_layoutJson; }
    void setLayoutJson(const QString &json);

    /// Aktives Pane innerhalb dieses Windows (-1 = keines).
    int activePaneId() const { return m_activePaneId; }
    void setActivePaneId(int paneId);

    /// Sessions dieses Windows in Baum-Blattreihenfolge. **Abgeleitet** aus
    /// `layoutJson` (single source of truth) — nicht separat gepflegt.
    Q_INVOKABLE QList<int> sessionIds() const;
    /// Pane-IDs in Blattreihenfolge (parallel zu sessionIds()).
    Q_INVOKABLE QList<int> paneIds() const;
    /// Zahl der Blätter im Baum.
    Q_INVOKABLE int paneCount() const;

    /// Monotoner Zähler für Window-IDs. Anders als `Session::nextId()` muss der Wert
    /// über Neustarts stabil bleiben — der Restore schreibt ihn deshalb mit
    /// `setNextId()` auf den höchsten geladenen Wert fort.
    static int nextId();
    /// Hebt den Zähler an, sodass der nächste `nextId()` **größer** als `id` ist.
    /// Kleinere Werte werden ignoriert (der Zähler geht nie zurück).
    static void setNextId(int id);

signals:
    void nameChanged();
    void groupChanged();
    void layoutChanged();
    void activePaneChanged();

private:
    /// Sammelt Blätter des Baums in Reihenfolge; `wantSession` wählt zwischen
    /// sessionId und paneId.
    QList<int> collectLeaves(bool wantSession) const;

    int m_id;
    QString m_name;
    QString m_group;
    QString m_layoutJson;
    int m_activePaneId = -1;
};

} // namespace qtmux

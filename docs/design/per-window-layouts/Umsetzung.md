# B1 — Per-Window-Layouts · Umsetzungsanweisung (QTMUX-83)

> Konkrete Blaupause zum freigegebenen Plan (`~/.claude/plans/quizzical-chasing-flame.md`).
> Diese Datei ist die **Koordinations-Grundlage**: die Modulschnitte hier definieren die
> disjunkten Teilaufgaben, die parallel (auch an Worker-Sessions) vergeben werden können.

## Kernidee (Wiederholung)
Sidebar listet **Windows** (Tabs). Jedes Window hat **seinen eigenen Split-Layout-Baum**.
Splits erzeugen Panes **im** Window (keine neue Sidebar-Kachel). Umschalten rendert den Baum
des aktiven Windows. **Blätter referenzieren Sessions per stabiler `sessionId`** (nicht mehr
`sessionRow`). `Session::id()` bleibt der MCP-Adress-Token.

---

## Stufe 1 — Datenmodell (C++). Diese Datei detailliert v. a. Stufe 1.

### Modul A — `Window` + `WindowModel` (NEU, `src/viewmodels/`)

**`Window : QObject`** — ein Sidebar-Eintrag. Besitzt Metadaten + den Layout-Baum als JSON
(die Authority liegt in C++, QML arbeitet mit einer deserialisierten Live-Kopie des AKTIVEN
Windows und schreibt bei Strukturänderung zurück).

```cpp
class Window : public QObject {
    Q_OBJECT
    Q_PROPERTY(int id READ id CONSTANT)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString group READ group WRITE setGroup NOTIFY groupChanged)
public:
    explicit Window(int id, QObject *parent = nullptr);
    int id() const { return m_id; }                       // persistierte, stabile Window-ID
    QString name() const; void setName(const QString&);   // auto aus aktivem Pane, umbenennbar
    QString group() const; void setGroup(const QString&); // Window-Gruppe (Stufe 5)
    QString layoutJson() const; void setLayoutJson(const QString&);  // Baum (Blätter=sessionId, +sizes)
    int activePaneId() const; void setActivePaneId(int);
    // Panes = Sessions dieses Windows. Reihenfolge = Baum-Blattreihenfolge.
    QList<int> sessionIds() const;                        // aus dem Baum abgeleitet ODER gepflegt
signals:
    void nameChanged(); void groupChanged(); void layoutChanged();
private:
    int m_id; QString m_name, m_group, m_layoutJson; int m_activePaneId = -1;
};
```
- **Window-ID:** eigener persistierter monotoner Zähler `Window::nextId()` (analog `Session::nextId`,
  aber der Höchstwert wird beim Restore aus den geladenen Windows fortgeschrieben — **muss**
  über Neustarts stabil sein, anders als `Session::id`).

**`WindowModel : QAbstractListModel`** — die Sidebar. Rollen:

| Rolle | QML-Name | Quelle |
|---|---|---|
| `IdRole` | `windowId` | `w->id()` |
| `TitleRole` | `title` | Name (bzw. Titel des aktiven Panes, wenn Name leer/auto) |
| `GroupRole` | `group` | `w->group()` |
| `PaneCountRole` | `paneCount` | Zahl der Blätter im Baum |
| `RunStateRole` | `runState` | **aggregiert** über die Panes (höchste Priorität: error>waiting>busy>idle) |
| `AttentionRole` | `needsAttention` | ODER über die Panes |
| `McpControllerRole`| `mcpController` | ODER über die Panes |

API (spiegelt heutige `SessionModel`-Formen, damit QML-Umbau minimal):
`count`, `windowAt(row)`, `windowById(id)`, `rowForId(id)`, `activeRow`/`setActiveRow`,
`createWindow()` (leeres Window, für „+"/`new_window`), `closeWindow(row)` (schließt alle Panes),
plus die Gruppen-Methoden in **Stufe 5** (`setWindowGroup`/`groups`/`groupSize`/`renameGroup`/
`moveGroup`/`moveGroupToRow` — 1:1 aus `SessionModel`).

### Modul B — `SessionModel` → globales Registry + `Session::windowId`

- `Session` (`src/core/Session.h`): Feld **`int m_windowId = -1`** + `windowId()`/`setWindowId()`
  (+ `Q_PROPERTY windowId`). Sonst unverändert.
- `SessionModel` **bleibt der flache Besitzer aller Sessions** (Lifetime, `wireSession`,
  `sessionById`/`rowForId`/`sessions()` — von MCP genutzt), **verliert aber die Sidebar-Rolle**
  (die Sidebar bindet künftig an `WindowModel`). Die Session-**Gruppen**-Logik
  (`setSessionGroup`/`regroupRow`/`moveBlock`/`moveGroupToRow`/`group`-Feld) wird nach
  `WindowModel` **verschoben** (Stufe 5) und aus `SessionModel` entfernt.
- Neue Helfer: `createShellSession(...)` etc. bekommen einen `windowId`-Parameter (oder der
  Aufrufer setzt `windowId` unmittelbar danach). `sessionsForWindow(int windowId)` → `QList<Session*>`.

### Modul C — Persistenz-Schema + Migration

**Neues Schema** (QSettings), ersetzt `sessions`-Array + `paneLayoutJson`:
```
windows/size = N
windows/<i>/id, name, group, activePaneId, layoutJson   // layoutJson: Baum, Blätter {paneId, sessionId, cfg{type,cwd,program,host,…}}
windows/activeRow
windows/nextWindowId, nextPaneId                          // Zähler-Fortschreibung
```
- Der `SessionConfig` (Typ/CWD/Programm/SSH-Parameter) wandert **ins Blatt** des Baums (jedes
  Pane trägt seine Session-Beschreibung).
- **Scrollback** `.ans` je Pane: Key = **`sessionId`** (nicht mehr Save-Index). `historyDir()`
  bleibt; `saveHistory()`/Restore auf sessionId umstellen.

**Migration (einmalig)** in `restoreState()`, wenn `windows/size` fehlt aber `sessions/size`
existiert: altes `sessions`-Array lesen → **je Eintrag ein Window** mit einem Pane
(`name` = Session-Titel, Blatt trägt den `SessionConfig`), altes `paneLayoutJson` **ignorieren**,
alte `.ans`-Dateien (Index-basiert) nach sessionId umbenennen/neu zuordnen (oder verwerfen —
akzeptierter einmaliger Verlust; im Doc als Entscheidung markiert: **verwerfen**, da die
Save-Index-Zuordnung nach der Migration nicht mehr trägt).

### Modul D — Unit-Tests
- `tst_windowmodel`: createWindow/closeWindow, windowById/rowForId, Panes-je-Window.
- `tst_migration`: altes `sessions`-Array (2–3 Einträge, teils gruppiert) → N Ein-Pane-Windows,
  Namen/Typen korrekt, Gruppen übernommen.
- `tst_windowgroups` (Stufe 5, ersetzt `tst_sessiongroups`): Contiguity-Invariante auf Windows.
- **Blatt-by-sessionId:** Test, dass Entfernen einer Session die Blätter korrekt beschneidet
  **ohne** Index-Remap (der ganze `onRowsRemoved`-Remap entfällt).

---

## Layout-Baum: Ownership & Blätter-by-sessionId
- Blatt neu: `{ paneId:int, sessionId:int, cfg:{…} }` (statt `{paneId, sessionRow}`); Split
  unverändert `{ orientation, children, sizes }`.
- **Ownership:** `Window::layoutJson` ist die Authority. Beim Window-Aktivieren deserialisiert
  QML den Baum des aktiven Windows in `window.activeTree` (JS), rendert ihn, und schreibt bei
  jeder Strukturänderung (`splitPane`/`closePane`/Drag/Proportionen) via
  `activeWindow.setLayoutJson(JSON.stringify(tree))` zurück. Persistenz = Windows serialisieren.
- `paneId` **global** (ein Zähler in `WindowModel` oder weiterhin QML, aber prozessweit eindeutig).

## MCP (Stufe 4, hier nur die Vertrags-Leitplanken)
- Unverändert by `Session::id()`: `send_text`, `read_screen`, `close_session`, `focus_session`,
  `attach_controller`, Event-Tools, `set_activity`.
- `create_session` → **neues Window + Pane**; `split_pane` → **Pane im aktiven Window**;
  beide geben Session-`id` zurück.
- `get_layout` → aktives Window (+ optional `windowId`); neu: `list_windows`/`focus_window`/
  `rename_window`/`close_window`/`new_window`. `assign_session` → deprecaten.
- Gruppen-Tools → Windows.

---

## Reihenfolge & Parallelisierbarkeit (Stufe 1)
1. **Zuerst Modul A** (Window/WindowModel-Interfaces) — definiert die Typen, gegen die B/C bauen.
2. Dann **parallel**: Modul B (SessionModel-Registry + `Session::windowId`) und Modul C
   (Persistenz-Schema + Migration gegen die A-Formen). Modul D (Tests) folgt je Modul.
3. QML-Verdrahtung (Stufe 2) erst NACH grünem Stufe-1-Build + Tests.

**Definition of Done Stufe 1:** Build grün, `ctest` inkl. neuer Tests grün, Migration eines
alten `sessions`-Profils erzeugt N Ein-Pane-Windows (Unit-getestet). Noch KEINE QML-/MCP-Änderung.

## Offene taktische Punkte (bei der Umsetzung zu entscheiden)
- `Window` in `qtmux_core` (gui-frei) oder App-Target? — voraussichtlich App-Target wie
  `SessionModel` (QML_ELEMENT), da eng mit der Sidebar/QML verbunden.
- `sessionIds()` aus dem Baum ableiten vs. separat pflegen (Redundanz/Sync). Empfehlung: **aus
  dem Baum ableiten** (single source of truth = layoutJson).
- Migration alter `.ans`: verwerfen (aktuell so entschieden) vs. best-effort nach sessionId.

## Fortschritt / Handoff (Stand 2026-07-28) — compact-fest

**Erledigt + gepusht:**
- **Stufe 1** (Datenmodell, additiv) — commit `f131f98`. `src/viewmodels/Window.{h,cpp}`
  (Q_PROPERTY `windowId` mit Getter `id()`; `name/group/layoutJson/activePaneId`; `sessionIds()`/
  `paneIds()`/`paneCount()` aus `layoutJson` abgeleitet; static `nextId()`/`setNextId()`),
  `WindowModel.{h,cpp}` (QAbstractListModel, `QML_ELEMENT`; `createWindow`/`closeWindow` →
  Signal `windowClosed(windowId, sessionIds)`; static `migrateSessionsToWindows(QSettings&)`),
  `Session::windowId` (additiv), `tests/tst_windowmodel.cpp` (7 Fälle inkl. Migration).
  ⚠ `Window` bewusst OHNE `QML_ELEMENT` (Namenskollision `QtQuick.Window.Window`) → über das
  Model als `QObject*`. Aggregat-Rollen runState/attention/mcpController noch TODO-Stub.
- **Screenshot-Harness** — commit `dbaa3c2`. GUI selbst prüfen ohne TCC:
  `QTMUX_PROFILE=shot QTMUX_MCP_PORT=7399 ./build/macos-test/qtmux.app/Contents/MacOS/qtmux
  --screenshot /tmp/…/x.png --settle 1800`, dann das PNG mit dem Read-Tool ansehen.
- **Stufe-2-Fundament:** `WindowModel { id: windows }` in `qml/Main.qml` instanziiert (baut,
  noch NICHT verdrahtet — App läuft weiter auf dem alten Modell).

**NÄCHSTER SCHRITT — Stufe 2 (QML-Flip), OFFEN:**
1. **C++ Windows-Restore (fehlt):** beim Start `migrateSessionsToWindows` laufen lassen, dann aus
   dem `windows`-Array je Window das `Window`-Objekt + je Blatt die Session (aus `cfg`) erzeugen
   (Gegenseite zur Migration; evtl. mit Stufe 3 zusammenziehen).
2. **QML-Rewire (atomar):** `SplitNode`-Root rendert den Baum des aktiven Windows; Blätter
   `session: sessions.sessionById(leaf.sessionId)` statt `sessionObject(sessionRow)`; `splitPane`/
   `newSession`/`closePane` aufs aktive Window; Sidebar `ListView.model` → `windows`, Delegate auf
   Window-Rollen, Klick = Window aktivieren; Startup/Shutdown umlegen; `paneId` global eindeutig.
   `assignToActivePane` entfällt.
3. Je Schritt: bauen + `--screenshot` prüfen; am Ende visuelle Abnahme durch den Anwender.

**Verifikation:** `ctest --test-dir build/macos-test` (17/17, inkl. `test_windowmodel`). MCP-e2e
gegen zweite Instanz. Worker #2/#3 im QTmux-Verzeichnis per QTmux-MCP steuerbar (disjunkte
C++-Dateien parallelisierbar, QML nicht).

**Referenzen:** Plan `~/.claude/plans/quizzical-chasing-flame.md`; Jira-Epic **QTMUX-83** (dual,
In Progress); Tasks #3 (Stufe 2, in Arbeit) … #6. Entscheidungen: Gruppen→Window-Gruppen,
Name auto+umbenennbar, Migration je Session→Ein-Pane-Window.

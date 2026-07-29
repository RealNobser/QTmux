#include "WindowModel.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

#include <algorithm>   // std::rotate, std::clamp

#include "RestoreMode.h"
#include "Window.h"

namespace qtmux {

WindowModel::WindowModel(QObject *parent) : QAbstractListModel(parent) {}

WindowModel::~WindowModel() {
    qDeleteAll(m_windows);
    m_windows.clear();
}

int WindowModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : count();
}

QHash<int, QByteArray> WindowModel::roleNames() const {
    return {
        {IdRole, "windowId"},
        {TitleRole, "title"},
        {GroupRole, "group"},
        {PaneCountRole, "paneCount"},
        {RunStateRole, "runState"},
        {AttentionRole, "needsAttention"},
        {McpControllerRole, "mcpController"},
        {WindowRole, "windowObject"},
    };
}

QVariant WindowModel::data(const QModelIndex &index, int role) const {
    if (index.row() < 0 || index.row() >= count()) return {};
    const Window *w = m_windows.at(index.row());
    switch (role) {
    case IdRole:
        return w->id();
    case TitleRole:
        // Automatischer Titel, solange kein Name gesetzt ist. Der Fallback „Titel des
        // aktiven Panes" braucht die Sessions und kommt mit Modul B hinzu.
        return w->name().isEmpty()
                   ? QCoreApplication::translate("WindowModel", "Fenster %1").arg(w->id())
                   : w->name();
    case GroupRole:
        return w->group();
    case PaneCountRole:
        return w->paneCount();
    // TODO (Modul B): über die Panes aggregieren, sobald `Session::windowId` steht —
    // runState nach Priorität error > waiting > busy > idle, die beiden Flags als ODER.
    // Bis dahin bewusst neutrale Werte statt einer halbrichtigen Aggregation.
    case RunStateRole:
        return 0;
    case AttentionRole:
        return false;
    case McpControllerRole:
        return false;
    case WindowRole:
        return QVariant::fromValue(const_cast<Window *>(w));
    default:
        return {};
    }
}

QObject *WindowModel::windowAt(int row) const {
    if (row < 0 || row >= count()) return nullptr;
    return m_windows.at(row);
}

Window *WindowModel::windowById(int id) const {
    for (Window *w : m_windows)
        if (w->id() == id) return w;
    return nullptr;
}

int WindowModel::rowForId(int id) const {
    for (int i = 0; i < count(); ++i)
        if (m_windows.at(i)->id() == id) return i;
    return -1;
}

void WindowModel::setActiveRow(int row) {
    if (row < -1 || row >= count()) return;
    if (m_activeRow == row) return;
    m_activeRow = row;
    emit activeRowChanged();
}

int WindowModel::createWindow(const QString &name) {
    auto *w = new Window(Window::nextId(), this);
    w->setName(name);
    const int row = count();
    beginInsertRows({}, row, row);
    m_windows.append(w);
    endInsertRows();
    wireWindow(w);
    emit countChanged();
    return row;
}

int WindowModel::createWindowWithId(int id, const QString &name, const QString &group) {
    auto *w = new Window(id, this);   // Window-Ctor schreibt den Zähler auf `id` fort
    w->setName(name);
    w->setGroup(group);
    const int row = count();
    beginInsertRows({}, row, row);
    m_windows.append(w);
    endInsertRows();
    wireWindow(w);
    emit countChanged();
    return row;
}

void WindowModel::closeWindow(int row) {
    if (row < 0 || row >= count()) return;
    Window *w = m_windows.at(row);
    const int id = w->id();
    const QList<int> sessions = w->sessionIds();

    beginRemoveRows({}, row, row);
    m_windows.removeAt(row);
    endRemoveRows();
    w->deleteLater();

    // Aktive Zeile nachführen: Zeilen unterhalb rutschen hoch, und die Auswahl bleibt
    // in der Liste (bzw. -1, wenn keine mehr da ist) — sonst zeigt sie ins Leere.
    int newActive = m_activeRow;
    if (row < newActive) --newActive;
    if (newActive >= count()) newActive = count() - 1;
    if (newActive != m_activeRow) {
        m_activeRow = newActive;
        emit activeRowChanged();
    }
    emit countChanged();
    emit windowClosed(id, sessions);
}

void WindowModel::wireWindow(Window *w) {
    connect(w, &Window::nameChanged, this, [this, w] { emitRowChanged(w, {TitleRole}); });
    connect(w, &Window::groupChanged, this, [this, w] { emitRowChanged(w, {GroupRole}); });
    // Der Baum bestimmt Panes UND (später) die aggregierten Rollen.
    connect(w, &Window::layoutChanged, this, [this, w] {
        emitRowChanged(w, {PaneCountRole, RunStateRole, AttentionRole, McpControllerRole});
    });
}

void WindowModel::emitRowChanged(const Window *w, const QList<int> &roles) {
    const int row = m_windows.indexOf(const_cast<Window *>(w));
    if (row < 0) return;
    emit dataChanged(index(row), index(row), roles);
}

void WindowModel::migrateSessionsToWindows(QSettings &s) {
    if (s.contains(QStringLiteral("windows/size"))) return;   // schon neues Schema
    const int n = s.beginReadArray(QStringLiteral("sessions"));
    if (n == 0) { s.endArray(); return; }                     // nichts Altes vorhanden

    struct Old {
        int type = 0, baud = 115200, sshPort = 22;
        QString serialPort, host, user, identity, workingDir, program, pluginId, pluginType, group;
    };
    QList<Old> olds;
    olds.reserve(n);
    for (int i = 0; i < n; ++i) {
        s.setArrayIndex(i);
        Old o;
        o.type       = s.value(QStringLiteral("type")).toInt();
        o.serialPort = s.value(QStringLiteral("serialPort")).toString();
        o.baud       = s.value(QStringLiteral("baud"), 115200).toInt();
        o.host       = s.value(QStringLiteral("host")).toString();
        o.sshPort    = s.value(QStringLiteral("sshPort"), 22).toInt();
        o.user       = s.value(QStringLiteral("user")).toString();
        o.identity   = s.value(QStringLiteral("identity")).toString();
        o.workingDir = s.value(QStringLiteral("workingDir")).toString();
        o.program    = s.value(QStringLiteral("program")).toString();
        o.pluginId   = s.value(QStringLiteral("pluginId")).toString();
        o.pluginType = s.value(QStringLiteral("pluginType")).toString();
        o.group      = s.value(QStringLiteral("group")).toString();
        olds.append(o);
    }
    s.endArray();
    const int oldActive = s.value(QStringLiteral("sessions/activeRow"), 0).toInt();

    // Je alte Session ein Ein-Pane-Window (Gruppe erhalten; Blatt trägt den SessionConfig).
    int nextWindowId = 1, nextPaneId = 1;
    s.beginWriteArray(QStringLiteral("windows"), olds.size());
    for (int i = 0; i < olds.size(); ++i) {
        const Old &o = olds.at(i);
        s.setArrayIndex(i);
        const int wid = nextWindowId++;
        const int pid = nextPaneId++;
        const QJsonObject cfg{
            {QStringLiteral("type"), o.type},           {QStringLiteral("serialPort"), o.serialPort},
            {QStringLiteral("baud"), o.baud},           {QStringLiteral("host"), o.host},
            {QStringLiteral("sshPort"), o.sshPort},     {QStringLiteral("user"), o.user},
            {QStringLiteral("identity"), o.identity},   {QStringLiteral("workingDir"), o.workingDir},
            {QStringLiteral("program"), o.program},     {QStringLiteral("pluginId"), o.pluginId},
            {QStringLiteral("pluginType"), o.pluginType},
        };
        const QJsonObject leaf{
            {QStringLiteral("paneId"), pid},
            {QStringLiteral("sessionId"), -1},   // Laufzeit-ID, beim Restore neu vergeben
            {QStringLiteral("cfg"), cfg},
        };
        s.setValue(QStringLiteral("id"), wid);
        s.setValue(QStringLiteral("name"), QString());        // leer = auto (Titel des Panes)
        s.setValue(QStringLiteral("group"), o.group);
        s.setValue(QStringLiteral("activePaneId"), pid);
        s.setValue(QStringLiteral("layoutJson"),
                   QString::fromUtf8(QJsonDocument(leaf).toJson(QJsonDocument::Compact)));
    }
    s.endArray();
    s.setValue(QStringLiteral("windows/activeRow"), oldActive < olds.size() ? oldActive : 0);
    s.setValue(QStringLiteral("windows/nextWindowId"), nextWindowId);
    s.setValue(QStringLiteral("windows/nextPaneId"), nextPaneId);
}

// --- Window-Gruppen (QTMUX-83, Stufe 5) — portiert aus SessionModel ----------

bool WindowModel::moveRowInternal(int from, int to) {
    if (from < 0 || from >= count() || to < 0 || to >= count() || from == to) return false;
    const int dest = to > from ? to + 1 : to;   // beginMoveRows: Abwärts-Ziel +1
    if (!beginMoveRows({}, from, from, {}, dest)) return false;
    m_windows.move(from, to);
    endMoveRows();
    if (m_activeRow == from) m_activeRow = to;
    else if (from < to && m_activeRow > from && m_activeRow <= to) --m_activeRow;
    else if (to < from && m_activeRow >= to && m_activeRow < from) ++m_activeRow;
    emit activeRowChanged();
    return true;
}

bool WindowModel::moveBlock(int first, int last, int dest) {
    if (first < 0 || last >= count() || first > last) return false;
    if (dest >= first && dest <= last + 1) return false;
    if (!beginMoveRows({}, first, last, {}, dest)) return false;
    Window *active = (m_activeRow >= 0 && m_activeRow < count()) ? m_windows.at(m_activeRow) : nullptr;
    if (dest < first)
        std::rotate(m_windows.begin() + dest, m_windows.begin() + first, m_windows.begin() + last + 1);
    else
        std::rotate(m_windows.begin() + first, m_windows.begin() + last + 1, m_windows.begin() + dest);
    endMoveRows();
    if (active) { m_activeRow = m_windows.indexOf(active); emit activeRowChanged(); }
    return true;
}

void WindowModel::moveGroup(const QString &name, int dir) {
    const QString g = name.trimmed();
    if (g.isEmpty() || dir == 0) return;
    int gStart = -1, gEnd = -1;
    for (int i = 0; i < count(); ++i)
        if (m_windows.at(i)->group() == g) { if (gStart < 0) gStart = i; gEnd = i; }
    if (gStart < 0) return;
    if (dir < 0 && gStart > 0)              moveGroupToRow(g, gStart - 1);
    else if (dir > 0 && gEnd < count() - 1) moveGroupToRow(g, gEnd + 1);
}

void WindowModel::moveGroupToRow(const QString &name, int targetRow) {
    const QString g = name.trimmed();
    if (g.isEmpty() || count() == 0) return;
    int gStart = -1, gEnd = -1;
    for (int i = 0; i < count(); ++i)
        if (m_windows.at(i)->group() == g) { if (gStart < 0) gStart = i; gEnd = i; }
    if (gStart < 0) return;
    targetRow = std::clamp(targetRow, 0, count() - 1);
    if (targetRow >= gStart && targetRow <= gEnd) return;
    const QString tv = m_windows.at(targetRow)->group();
    int tStart = targetRow, tEnd = targetRow;
    while (tStart > 0 && m_windows.at(tStart - 1)->group() == tv) --tStart;
    while (tEnd < count() - 1 && m_windows.at(tEnd + 1)->group() == tv) ++tEnd;
    bool moved = false;
    if (tEnd < gStart)       moved = moveBlock(gStart, gEnd, tStart);
    else if (tStart > gEnd)  moved = moveBlock(gStart, gEnd, tEnd + 1);
    if (moved) emit groupsChanged();
}

void WindowModel::moveWindow(int from, int to) {
    if (!moveRowInternal(from, to)) return;
    // Gruppe der neuen Nachbarschaft übernehmen (zusammenhängende Section-Blöcke).
    const QString neighbour = to > 0 ? m_windows.at(to - 1)->group()
                            : (count() > 1 ? m_windows.at(1)->group() : QString());
    if (m_windows.at(to)->group() != neighbour) {
        m_windows.at(to)->setGroup(neighbour);
        emitRowChanged(m_windows.at(to), {GroupRole});
        emit groupsChanged();
    }
}

int WindowModel::regroupRow(int row) {
    const QString g = m_windows.at(row)->group();
    int last = -1;
    for (int i = 0; i < count(); ++i)
        if (i != row && m_windows.at(i)->group() == g) last = i;
    int to = row;
    if (last >= 0) {
        to = (last > row) ? last : last + 1;
    } else if (row > 0 && row < count() - 1
               && !m_windows.at(row - 1)->group().isEmpty()
               && m_windows.at(row - 1)->group() == m_windows.at(row + 1)->group()
               && m_windows.at(row - 1)->group() != g) {
        to = count() - 1;
    }
    if (to == row) return row;
    return moveRowInternal(row, to) ? to : row;
}

void WindowModel::setWindowGroup(int row, const QString &name) {
    if (row < 0 || row >= count()) return;
    Window *w = m_windows.at(row);
    if (w->group() == name.trimmed()) return;
    w->setGroup(name);
    const int newRow = regroupRow(row);            // erst umsortieren, dann am Zielindex melden
    emitRowChanged(m_windows.at(newRow), {GroupRole});
    emit groupsChanged();
}

QStringList WindowModel::groups() const {
    QStringList out;
    for (const Window *w : m_windows)
        if (!w->group().isEmpty() && !out.contains(w->group())) out << w->group();
    return out;
}

int WindowModel::groupSize(const QString &name) const {
    int n = 0;
    for (const Window *w : m_windows)
        if (w->group() == name) ++n;
    return n;
}

void WindowModel::renameGroup(const QString &from, const QString &to) {
    const QString src = from.trimmed();
    if (src.isEmpty()) return;
    const QString dst = to.trimmed();
    bool touched = false;
    for (int i = 0; i < count(); ++i) {
        if (m_windows.at(i)->group() != src) continue;
        m_windows.at(i)->setGroup(dst);
        emitRowChanged(m_windows.at(i), {GroupRole});
        touched = true;
    }
    if (!touched) return;
    for (int i = 0; i < count(); ++i)
        if (m_windows.at(i)->group() == dst) regroupRow(i);
    emit groupsChanged();
}

void WindowModel::runMigration() {
    QSettings s;
    migrateSessionsToWindows(s);
}

void WindowModel::writeWindows(const QVariantList &wins, int activeRow, int nextPaneId) {
    QSettings s;
    // Altes flaches Session-Schema entfernen — ab jetzt ist `windows` maßgeblich.
    s.remove(QStringLiteral("sessions"));
    s.remove(QStringLiteral("windows"));
    s.beginWriteArray(QStringLiteral("windows"), static_cast<int>(wins.size()));
    for (int i = 0; i < wins.size(); ++i) {
        const QVariantMap w = wins.at(i).toMap();
        s.setArrayIndex(i);
        s.setValue(QStringLiteral("id"), w.value(QStringLiteral("id")).toInt());
        s.setValue(QStringLiteral("name"), w.value(QStringLiteral("name")).toString());
        s.setValue(QStringLiteral("group"), w.value(QStringLiteral("group")).toString());
        s.setValue(QStringLiteral("activePaneId"), w.value(QStringLiteral("activePaneId")).toInt());
        s.setValue(QStringLiteral("layoutJson"), w.value(QStringLiteral("layoutJson")).toString());
    }
    s.endArray();
    s.setValue(QStringLiteral("windows/activeRow"), activeRow);
    s.setValue(QStringLiteral("windows/nextPaneId"), nextPaneId);
}

QVariantMap WindowModel::readWindows() const {
    QSettings s;
    QVariantMap out;
    out[QStringLiteral("present")] = s.contains(QStringLiteral("windows/size"));
    QVariantList list;
    const int n = s.beginReadArray(QStringLiteral("windows"));
    for (int i = 0; i < n; ++i) {
        s.setArrayIndex(i);
        QVariantMap w;
        w[QStringLiteral("id")]           = s.value(QStringLiteral("id")).toInt();
        w[QStringLiteral("name")]         = s.value(QStringLiteral("name")).toString();
        w[QStringLiteral("group")]        = s.value(QStringLiteral("group")).toString();
        w[QStringLiteral("activePaneId")] = s.value(QStringLiteral("activePaneId")).toInt();
        w[QStringLiteral("layoutJson")]   = s.value(QStringLiteral("layoutJson")).toString();
        list.append(w);
    }
    s.endArray();
    out[QStringLiteral("windows")]    = list;
    out[QStringLiteral("activeRow")]  = s.value(QStringLiteral("windows/activeRow"), 0).toInt();
    out[QStringLiteral("nextPaneId")] = s.value(QStringLiteral("windows/nextPaneId"), 1).toInt();
    return out;
}

// --- Umfang der Wiederherstellung (QTMUX-99) -------------------------------------
// Reine Brücken nach RestoreMode.h; die Begründungen stehen dort.

bool WindowModel::restoresLayout(int mode) const {
    return qtmux::restoresLayout(restoreModeFromInt(mode));
}

bool WindowModel::restoresHistory(int mode) const {
    return qtmux::restoresHistory(restoreModeFromInt(mode));
}

bool WindowModel::persistsOnQuit(int mode) const {
    return qtmux::persistsOnQuit(restoreModeFromInt(mode));
}

} // namespace qtmux

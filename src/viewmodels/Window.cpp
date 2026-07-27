#include "Window.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace qtmux {
namespace {

/// Ein Knoten ist ein Split, sobald er ein `children`-Array trägt — alles andere
/// ist ein Blatt (dieselbe Unterscheidung wie im QML-Baum, `isLeaf`).
bool isSplit(const QJsonObject &node) {
    return node.value(QStringLiteral("children")).isArray();
}

void walk(const QJsonValue &node, bool wantSession, QList<int> &out) {
    if (!node.isObject()) return;
    const QJsonObject obj = node.toObject();
    if (isSplit(obj)) {
        const QJsonArray children = obj.value(QStringLiteral("children")).toArray();
        for (const QJsonValue &child : children) walk(child, wantSession, out);
        return;
    }
    const QString key = wantSession ? QStringLiteral("sessionId") : QStringLiteral("paneId");
    const QJsonValue v = obj.value(key);
    if (v.isDouble()) out << v.toInt();
}

} // namespace

Window::Window(int id, QObject *parent) : QObject(parent), m_id(id) {
    // Ein von außen gesetztes (z. B. restauriertes) ID darf den Zähler nicht
    // unterlaufen — sonst vergäbe der nächste createWindow() dieselbe ID.
    setNextId(id);
}

namespace {
/// Genau EIN Zählerzustand für nextId()/setNextId().
int &idCounter() {
    static int counter = 0;
    return counter;
}
} // namespace

int Window::nextId() { return ++idCounter(); }

void Window::setNextId(int id) {
    // Der Zähler geht nie zurück: nach setNextId(n) liefert nextId() garantiert > n.
    if (id > idCounter()) idCounter() = id;
}

void Window::setName(const QString &name) {
    if (m_name == name) return;
    m_name = name;
    emit nameChanged();
}

void Window::setGroup(const QString &group) {
    if (m_group == group) return;
    m_group = group;
    emit groupChanged();
}

void Window::setLayoutJson(const QString &json) {
    if (m_layoutJson == json) return;
    m_layoutJson = json;
    emit layoutChanged();
}

void Window::setActivePaneId(int paneId) {
    if (m_activePaneId == paneId) return;
    m_activePaneId = paneId;
    emit activePaneChanged();
}

QList<int> Window::collectLeaves(bool wantSession) const {
    QList<int> out;
    if (m_layoutJson.isEmpty()) return out;
    const QJsonDocument doc = QJsonDocument::fromJson(m_layoutJson.toUtf8());
    if (doc.isNull() || !doc.isObject()) return out;   // "null" / kaputt = leeres Window
    walk(doc.object(), wantSession, out);
    return out;
}

QList<int> Window::sessionIds() const { return collectLeaves(true); }
QList<int> Window::paneIds() const { return collectLeaves(false); }
int Window::paneCount() const { return static_cast<int>(collectLeaves(false).size()); }

} // namespace qtmux

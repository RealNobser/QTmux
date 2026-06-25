#include "HotkeyRegistry.h"

#include <QSettings>

namespace qtmux {

HotkeyRegistry *HotkeyRegistry::instance() {
    static HotkeyRegistry reg;
    return &reg;
}

HotkeyRegistry::HotkeyRegistry(QObject *parent) : QObject(parent) {
    // Standard-Belegung. Reihenfolge = Anzeige in den Einstellungen.
    // Nur die String-basierten Shortcuts sind konfigurierbar; Zoom +/- und Copy/Paste
    // bleiben bewusst auf StandardKey (plattform-/terminal-sensibel) und nicht hier.
    m_defaults = {
        {QStringLiteral("actNewSession"),     QStringLiteral("Ctrl+T")},
        {QStringLiteral("actCloseSession"),   QStringLiteral("Ctrl+W")},
        {QStringLiteral("actClosePane"),      QStringLiteral("Ctrl+Shift+W")},
        // Session-Navigation: auf macOS heißt Qt-"Ctrl" Cmd, und Cmd+Tab gehört dem
        // System-App-Switcher (die App sieht die Taste nie). "Meta+Tab" = PHYSISCHES
        // Ctrl+Tab — die plattformübliche Belegung (Terminal.app/iTerm-Stil).
#if defined(Q_OS_MACOS)
        {QStringLiteral("actNextSession"),    QStringLiteral("Meta+Tab")},
        {QStringLiteral("actPrevSession"),    QStringLiteral("Meta+Shift+Tab")},
#else
        {QStringLiteral("actNextSession"),    QStringLiteral("Ctrl+Tab")},
        {QStringLiteral("actPrevSession"),    QStringLiteral("Ctrl+Shift+Tab")},
#endif
        {QStringLiteral("actSplitH"),         QStringLiteral("Ctrl+Shift+E")},
        {QStringLiteral("actSplitV"),         QStringLiteral("Ctrl+Shift+O")},
        {QStringLiteral("actCommandPalette"), QStringLiteral("Ctrl+K")},
        {QStringLiteral("actBroadcast"),      QStringLiteral("Ctrl+Shift+B")},
        {QStringLiteral("actNewSsh"),         QStringLiteral("Ctrl+Shift+S")},
        {QStringLiteral("actNewSerial"),      QStringLiteral("Ctrl+Shift+R")},
        {QStringLiteral("actConnections"),    QStringLiteral("Ctrl+Shift+M")},
        {QStringLiteral("actVault"),          QStringLiteral("Ctrl+Shift+K")},
        {QStringLiteral("actMcpToggle"),      QStringLiteral("Ctrl+Shift+A")},
        {QStringLiteral("actZoomReset"),      QStringLiteral("Ctrl+0")},
        {QStringLiteral("actToggleTheme"),    QStringLiteral("Ctrl+D")},
        {QStringLiteral("actSettings"),       QStringLiteral("Ctrl+,")},
        // Bewusst OHNE Standard-Kürzel: F1 (und alle F-Tasten) gehören im Terminal der
        // Shell/Clink. „Über" bleibt über Menü „Hilfe" + Befehlspalette erreichbar; ein
        // eigenes Kürzel kann in den Einstellungen vergeben werden.
        {QStringLiteral("actAbout"),          QString()},
        {QStringLiteral("actQuit"),           QStringLiteral("Ctrl+Q")},
    };
    loadOverrides();
}

int HotkeyRegistry::defaultIndex(const QString &id) const {
    for (int i = 0; i < m_defaults.size(); ++i)
        if (m_defaults.at(i).first == id) return i;
    return -1;
}

QStringList HotkeyRegistry::actionIds() const {
    QStringList ids;
    for (const auto &p : m_defaults) ids << p.first;
    return ids;
}

QString HotkeyRegistry::defaultSequence(const QString &id) const {
    const int i = defaultIndex(id);
    return i >= 0 ? m_defaults.at(i).second : QString();
}

QString HotkeyRegistry::sequence(const QString &id) const {
    if (m_overrides.contains(id)) return m_overrides.value(id);
    return defaultSequence(id);
}

bool HotkeyRegistry::isCustom(const QString &id) const {
    return m_overrides.contains(id);
}

QVariantMap HotkeyRegistry::bindings() const {
    QVariantMap m;
    for (const auto &p : m_defaults)
        m[p.first] = m_overrides.contains(p.first) ? m_overrides.value(p.first) : p.second;
    return m;
}

void HotkeyRegistry::setBinding(const QString &id, const QString &seq) {
    if (defaultIndex(id) < 0) return;   // unbekannte Aktion
    const QString s = seq.trimmed();
    // Leer oder identisch zum Default → Override entfernen (zurück zur Werkseinstellung).
    if (s.isEmpty() || QString::compare(s, defaultSequence(id), Qt::CaseInsensitive) == 0) {
        if (!m_overrides.remove(id)) return;   // war ohnehin Default
    } else {
        if (m_overrides.value(id) == s) return;  // unverändert
        m_overrides[id] = s;
    }
    persist();
    emit changed();
}

void HotkeyRegistry::reset(const QString &id) {
    if (m_overrides.remove(id) > 0) {
        persist();
        emit changed();
    }
}

void HotkeyRegistry::resetAll() {
    if (m_overrides.isEmpty()) return;
    m_overrides.clear();
    persist();
    emit changed();
}

QString HotkeyRegistry::conflict(const QString &seq, const QString &exceptId) const {
    const QString s = seq.trimmed();
    if (s.isEmpty()) return QString();
    for (const auto &p : m_defaults) {
        if (p.first == exceptId) continue;
        if (QString::compare(sequence(p.first), s, Qt::CaseInsensitive) == 0)
            return p.first;
    }
    return QString();
}

void HotkeyRegistry::loadOverrides() {
    QSettings s;
    s.beginGroup(QStringLiteral("hotkeys"));
    const QStringList keys = s.childKeys();
    for (const QString &id : keys) {
        if (defaultIndex(id) < 0) continue;   // veraltete IDs überspringen
        const QString seq = s.value(id).toString().trimmed();
        if (!seq.isEmpty() && QString::compare(seq, defaultSequence(id), Qt::CaseInsensitive) != 0)
            m_overrides[id] = seq;
    }
    s.endGroup();
}

void HotkeyRegistry::persist() const {
    QSettings s;
    s.remove(QStringLiteral("hotkeys"));   // nur aktuelle Overrides schreiben
    s.beginGroup(QStringLiteral("hotkeys"));
    for (auto it = m_overrides.constBegin(); it != m_overrides.constEnd(); ++it)
        s.setValue(it.key(), it.value());
    s.endGroup();
}

} // namespace qtmux

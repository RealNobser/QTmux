#include "Theme.h"
#include <QSettings>

namespace qtmux {

Theme::Theme(QObject *parent) : QObject(parent) {
    QSettings s;
    m_dark = s.value(QStringLiteral("ui/darkMode"), true).toBool();
}

void Theme::setDark(bool dark) {
    if (dark == m_dark) return;
    m_dark = dark;
    QSettings().setValue(QStringLiteral("ui/darkMode"), dark);
    emit changed();
}

// Dark- bzw. Light-Wert je nach Modus.
QColor Theme::bgSidebar() const       { return m_dark ? QColor(0x16, 0x17, 0x1f) : QColor(0xec, 0xed, 0xf2); }
QColor Theme::bgMain() const          { return m_dark ? QColor(0x1e, 0x1f, 0x29) : QColor(0xfa, 0xfb, 0xfd); }
QColor Theme::bgElevated() const      { return m_dark ? QColor(0x22, 0x24, 0x33) : QColor(0xff, 0xff, 0xff); }
QColor Theme::sidebarHover() const    { return m_dark ? QColor(0x26, 0x28, 0x38) : QColor(0xe1, 0xe4, 0xec); }
QColor Theme::sidebarSelected() const { return m_dark ? QColor(0x2b, 0x2e, 0x42) : QColor(0xd6, 0xdf, 0xff); }
QColor Theme::border() const          { return m_dark ? QColor(0x3a, 0x3f, 0x5c) : QColor(0xc9, 0xcd, 0xd8); }
QColor Theme::accent() const          { return QColor(0x5b, 0x8c, 0xff); }
QColor Theme::textBright() const      { return m_dark ? QColor(0xe6, 0xe7, 0xee) : QColor(0x1b, 0x1d, 0x26); }
QColor Theme::textDim() const         { return m_dark ? QColor(0x8a, 0x8d, 0x9a) : QColor(0x6a, 0x6e, 0x7c); }

} // namespace qtmux

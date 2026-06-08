#include "Theme.h"
#include "ColorScheme.h"
#include <QSettings>
#include <QGuiApplication>
#include <QStyleHints>

namespace qtmux {

Theme::Theme(QObject *parent) : QObject(parent) {
    QSettings s;
    m_mode = static_cast<Mode>(s.value(QStringLiteral("ui/themeMode"), static_cast<int>(System)).toInt());

    // Auf OS-Wechsel (hell/dunkel) live reagieren. Immer melden: im System-Modus
    // ändert sich die ganze Palette, sonst zumindest systemDark/menuIcon (native Menüs).
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            [this](Qt::ColorScheme) { emit changed(); });

    // Terminal-Farben folgen dem gewählten Farbschema → bei dessen Wechsel neu binden.
    connect(ColorSchemeRegistry::instance(), &ColorSchemeRegistry::changed, this,
            [this]() { emit changed(); });
}

void Theme::setMode(Mode mode) {
    if (mode == m_mode) return;
    m_mode = mode;
    QSettings().setValue(QStringLiteral("ui/themeMode"), static_cast<int>(mode));
    emit changed();
}

bool Theme::dark() const {
    switch (m_mode) {
    case Dark:  return true;
    case Light: return false;
    case System:
    default:
        return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    }
}

bool Theme::systemDark() const {
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

// Tönung für Icons in NATIVEN Menüs: folgt dem OS-Schema, nicht dem App-Theme,
// damit Icons auf der (immer system-gefärbten) macOS-Menüleiste sichtbar bleiben.
QColor Theme::menuIcon() const {
    return systemDark() ? QColor(0xEC, 0xEC, 0xEC) : QColor(0x26, 0x26, 0x26);
}

void Theme::toggle() { setMode(dark() ? Light : Dark); }

// Dark- bzw. Light-Wert je nach effektivem Modus.
QColor Theme::bgSidebar() const       { return dark() ? QColor(0x16, 0x17, 0x1f) : QColor(0xec, 0xed, 0xf2); }
QColor Theme::bgMain() const          { return dark() ? QColor(0x1e, 0x1f, 0x29) : QColor(0xfa, 0xfb, 0xfd); }
QColor Theme::bgElevated() const      { return dark() ? QColor(0x22, 0x24, 0x33) : QColor(0xff, 0xff, 0xff); }
QColor Theme::sidebarHover() const    { return dark() ? QColor(0x26, 0x28, 0x38) : QColor(0xe1, 0xe4, 0xec); }
QColor Theme::sidebarSelected() const { return dark() ? QColor(0x2b, 0x2e, 0x42) : QColor(0xd6, 0xdf, 0xff); }
QColor Theme::border() const          { return dark() ? QColor(0x3a, 0x3f, 0x5c) : QColor(0xc9, 0xcd, 0xd8); }
QColor Theme::accent() const          { return QColor(0x5b, 0x8c, 0xff); }
QColor Theme::textBright() const      { return dark() ? QColor(0xe6, 0xe7, 0xee) : QColor(0x1b, 0x1d, 0x26); }
QColor Theme::textDim() const         { return dark() ? QColor(0x8a, 0x8d, 0x9a) : QColor(0x6a, 0x6e, 0x7c); }
// Terminal-Default-Flächen (nicht-gefärbte Zellen) + Cursor folgen dem gewählten
// Farbschema (QTMUX-18), unabhängig vom App-Hell/Dunkel.
QColor Theme::terminalBg() const     { return QColor::fromRgb(ColorSchemeRegistry::instance()->currentScheme().bg); }
QColor Theme::terminalFg() const     { return QColor::fromRgb(ColorSchemeRegistry::instance()->currentScheme().fg); }
QColor Theme::terminalCursor() const { return QColor::fromRgb(ColorSchemeRegistry::instance()->currentScheme().cursor); }

} // namespace qtmux

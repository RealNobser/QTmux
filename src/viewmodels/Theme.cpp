#include "Theme.h"
#include "ColorScheme.h"
#include <QSettings>
#include <QGuiApplication>
#include <QStyleHints>

namespace qtmux {

namespace {
// Lineare Mischung zweier Farben (t=0 → a, t=1 → b).
QColor mix(const QColor &a, const QColor &b, qreal t) {
    return QColor::fromRgbF(a.redF()   + (b.redF()   - a.redF())   * t,
                            a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF()  + (b.blueF()  - a.blueF())  * t);
}
} // namespace

Theme::Theme(QObject *parent) : QObject(parent) {
    QSettings s;
    m_mode = static_cast<Mode>(s.value(QStringLiteral("ui/themeMode"), static_cast<int>(System)).toInt());

    // Effektiven Hell/Dunkel-Modus an die Schema-Registry melden (wählt das aktive Schema).
    ColorSchemeRegistry::instance()->setDark(dark());

    // Auf OS-Wechsel (hell/dunkel) live reagieren: Modus an die Registry melden und
    // die ganze Palette neu binden lassen.
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            [this](Qt::ColorScheme) {
                ColorSchemeRegistry::instance()->setDark(dark());
                emit changed();
            });

    // Die ganze App folgt dem gewählten Schema → bei dessen Wechsel neu binden.
    connect(ColorSchemeRegistry::instance(), &ColorSchemeRegistry::changed, this,
            [this]() { emit changed(); });
}

void Theme::setMode(Mode mode) {
    if (mode == m_mode) return;
    m_mode = mode;
    QSettings().setValue(QStringLiteral("ui/themeMode"), static_cast<int>(mode));
    ColorSchemeRegistry::instance()->setDark(dark());
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

// Tönung für Menü-Icons. macOS rendert eine NATIVE Menüleiste, die immer dem
// OS-Schema folgt → dort muss die Icon-Tönung an systemDark hängen. Auf Windows/
// Linux ist die Menüleiste eine In-Window-QML-MenuBar, die dem APP-Theme folgt →
// dort an dark() hängen (sonst sind die Icons z. B. bei App=Dunkel/System=Hell
// dunkel auf dunklem Grund und kaum sichtbar).
QColor Theme::menuIcon() const {
#ifdef Q_OS_MACOS
    const bool d = systemDark();
#else
    const bool d = dark();
#endif
    return d ? QColor(0xEC, 0xEC, 0xEC) : QColor(0x26, 0x26, 0x26);
}

void Theme::toggle() { setMode(dark() ? Light : Dark); }

// --- Chrome-Farben: aus dem aktiven Farbschema abgeleitet (QTMUX-18) ---------
// Die GANZE App folgt dem Schema: Flächen sind Schattierungen von bg→fg (funktioniert
// für helle wie dunkle Schemata gleich), Akzent = ANSI-Blau (Index 4).
QColor Theme::bgSidebar() const       { const ColorScheme &s = ColorSchemeRegistry::instance()->currentScheme(); return mix(QColor::fromRgb(s.bg), QColor::fromRgb(s.fg), 0.05); }
QColor Theme::bgMain() const          { return QColor::fromRgb(ColorSchemeRegistry::instance()->currentScheme().bg); }
QColor Theme::bgElevated() const      { const ColorScheme &s = ColorSchemeRegistry::instance()->currentScheme(); return mix(QColor::fromRgb(s.bg), QColor::fromRgb(s.fg), 0.10); }
QColor Theme::sidebarHover() const    { const ColorScheme &s = ColorSchemeRegistry::instance()->currentScheme(); return mix(QColor::fromRgb(s.bg), QColor::fromRgb(s.fg), 0.16); }
QColor Theme::sidebarSelected() const { const ColorScheme &s = ColorSchemeRegistry::instance()->currentScheme(); return mix(QColor::fromRgb(s.bg), QColor::fromRgb(s.ansi[4]), 0.30); }
QColor Theme::border() const          { const ColorScheme &s = ColorSchemeRegistry::instance()->currentScheme(); return mix(QColor::fromRgb(s.bg), QColor::fromRgb(s.fg), 0.24); }
QColor Theme::accent() const          { return QColor::fromRgb(ColorSchemeRegistry::instance()->currentScheme().ansi[4]); }
QColor Theme::textBright() const      { return QColor::fromRgb(ColorSchemeRegistry::instance()->currentScheme().fg); }
QColor Theme::textDim() const         { const ColorScheme &s = ColorSchemeRegistry::instance()->currentScheme(); return mix(QColor::fromRgb(s.fg), QColor::fromRgb(s.bg), 0.45); }
// Terminal-Flächen + Cursor = Schema-Farben direkt.
QColor Theme::terminalBg() const     { return QColor::fromRgb(ColorSchemeRegistry::instance()->currentScheme().bg); }
QColor Theme::terminalFg() const     { return QColor::fromRgb(ColorSchemeRegistry::instance()->currentScheme().fg); }
QColor Theme::terminalCursor() const { return QColor::fromRgb(ColorSchemeRegistry::instance()->currentScheme().cursor); }

} // namespace qtmux

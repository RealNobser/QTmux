#include "ColorScheme.h"

#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QUrl>
#include <QVariantList>
#include <QXmlStreamReader>
#include <QRegularExpression>

namespace qtmux {

namespace {

QString hex(quint32 rgb) {
    return QStringLiteral("#%1").arg(rgb & 0xffffff, 6, 16, QChar('0'));
}

// Baut ein Schema aus name + dark + fg/bg/cursor + 16 ANSI-Farben.
ColorScheme make(const QString &name, bool dark, quint32 fg, quint32 bg, quint32 cursor,
                 std::initializer_list<quint32> ansi) {
    ColorScheme s;
    s.name = name;
    s.dark = dark;
    s.builtin = true;
    s.fg = fg;
    s.bg = bg;
    s.cursor = cursor;
    int i = 0;
    for (quint32 c : ansi) {
        if (i < 16) s.ansi[i++] = c;
    }
    return s;
}

} // namespace

ColorSchemeRegistry *ColorSchemeRegistry::instance() {
    static ColorSchemeRegistry inst;
    return &inst;
}

ColorSchemeRegistry::ColorSchemeRegistry(QObject *parent) : QObject(parent) {
    loadBuiltins();
    loadPersisted();
    const QString firstName = m_schemes.isEmpty() ? QString() : m_schemes.first().name;
    if (indexOf(m_darkChoice) < 0)  m_darkChoice  = firstName;
    if (indexOf(m_lightChoice) < 0) m_lightChoice = firstName;
}

void ColorSchemeRegistry::loadBuiltins() {
    m_schemes.clear();
    m_schemes << make(QStringLiteral("QTmux Dunkel"), true, 0xe6e7ee, 0x1e1f29, 0xe6e7ee, {
        0x2b2d3a, 0xe5534b, 0x46d369, 0xf5c451, 0x5b8cff, 0xb072ff, 0x4cd4d4, 0xc8cad6,
        0x4a4d5e, 0xff6b63, 0x6ef08c, 0xffd76b, 0x80a8ff, 0xc99bff, 0x7ee8e8, 0xffffff });
    m_schemes << make(QStringLiteral("QTmux Hell"), false, 0x1b1d26, 0xfdfdfd, 0x1b1d26, {
        0x2b2d3a, 0xd13b34, 0x2fa84f, 0xb8860b, 0x2f6df5, 0x9333ea, 0x0e9aa7, 0xc8cad6,
        0x6a6e7c, 0xe5534b, 0x46d369, 0xf5c451, 0x5b8cff, 0xb072ff, 0x4cd4d4, 0x1b1d26 });
    m_schemes << make(QStringLiteral("Solarized Dark"), true, 0x839496, 0x002b36, 0x93a1a1, {
        0x073642, 0xdc322f, 0x859900, 0xb58900, 0x268bd2, 0xd33682, 0x2aa198, 0xeee8d5,
        0x002b36, 0xcb4b16, 0x586e75, 0x657b83, 0x839496, 0x6c71c4, 0x93a1a1, 0xfdf6e3 });
    m_schemes << make(QStringLiteral("Solarized Hell"), false, 0x657b83, 0xfdf6e3, 0x586e75, {
        0x073642, 0xdc322f, 0x859900, 0xb58900, 0x268bd2, 0xd33682, 0x2aa198, 0xeee8d5,
        0x002b36, 0xcb4b16, 0x586e75, 0x657b83, 0x839496, 0x6c71c4, 0x93a1a1, 0xfdf6e3 });
    m_schemes << make(QStringLiteral("Dracula"), true, 0xf8f8f2, 0x282a36, 0xf8f8f2, {
        0x21222c, 0xff5555, 0x50fa7b, 0xf1fa8c, 0xbd93f9, 0xff79c6, 0x8be9fd, 0xf8f8f2,
        0x6272a4, 0xff6e6e, 0x69ff94, 0xffffa5, 0xd6acff, 0xff92df, 0xa4ffff, 0xffffff });
    m_schemes << make(QStringLiteral("Gruvbox Dunkel"), true, 0xebdbb2, 0x282828, 0xebdbb2, {
        0x282828, 0xcc241d, 0x98971a, 0xd79921, 0x458588, 0xb16286, 0x689d6a, 0xa89984,
        0x928374, 0xfb4934, 0xb8bb26, 0xfabd2f, 0x83a598, 0xd3869b, 0x8ec07c, 0xebdbb2 });
    m_schemes << make(QStringLiteral("Nord"), true, 0xd8dee9, 0x2e3440, 0xd8dee9, {
        0x3b4252, 0xbf616a, 0xa3be8c, 0xebcb8b, 0x81a1c1, 0xb48ead, 0x88c0d0, 0xe5e9f0,
        0x4c566a, 0xbf616a, 0xa3be8c, 0xebcb8b, 0x81a1c1, 0xb48ead, 0x8fbcbb, 0xeceff4 });
    m_schemes << make(QStringLiteral("One Dark"), true, 0xabb2bf, 0x282c34, 0xabb2bf, {
        0x282c34, 0xe06c75, 0x98c379, 0xe5c07b, 0x61afef, 0xc678dd, 0x56b6c2, 0xabb2bf,
        0x5c6370, 0xe06c75, 0x98c379, 0xe5c07b, 0x61afef, 0xc678dd, 0x56b6c2, 0xffffff });
}

// --- Persistenz -------------------------------------------------------------
void ColorSchemeRegistry::loadPersisted() {
    QSettings st;
    st.beginGroup(QStringLiteral("colorSchemes"));
    m_darkChoice  = st.value(QStringLiteral("dark"),  QStringLiteral("QTmux Dunkel")).toString();
    m_lightChoice = st.value(QStringLiteral("light"), QStringLiteral("QTmux Hell")).toString();
    // Importierte Schemata: je ein String "name|fg|bg|cursor|dark|a0,…,a15" (hex ohne #).
    const QStringList imported = st.value(QStringLiteral("imported")).toStringList();
    for (const QString &line : imported) {
        const QStringList p = line.split(QChar('|'));
        if (p.size() < 6) continue;
        ColorScheme s;
        s.name = p.at(0);
        s.builtin = false;
        bool ok = false;
        s.fg = p.at(1).toUInt(&ok, 16);
        s.bg = p.at(2).toUInt(&ok, 16);
        s.cursor = p.at(3).toUInt(&ok, 16);
        s.dark = p.at(4).toInt() != 0;
        const QStringList a = p.at(5).split(QChar(','));
        for (int i = 0; i < 16 && i < a.size(); ++i) s.ansi[i] = a.at(i).toUInt(&ok, 16);
        if (indexOf(s.name) < 0) m_schemes << s;
    }
    st.endGroup();
}

void ColorSchemeRegistry::persist() const {
    QSettings st;
    st.beginGroup(QStringLiteral("colorSchemes"));
    st.setValue(QStringLiteral("dark"), m_darkChoice);
    st.setValue(QStringLiteral("light"), m_lightChoice);
    QStringList imported;
    for (const ColorScheme &s : m_schemes) {
        if (s.builtin) continue;
        QStringList a;
        for (int i = 0; i < 16; ++i) a << QString::number(s.ansi[i], 16);
        imported << QStringList{ s.name,
                                 QString::number(s.fg, 16), QString::number(s.bg, 16),
                                 QString::number(s.cursor, 16), QString::number(s.dark ? 1 : 0),
                                 a.join(QChar(',')) }.join(QChar('|'));
    }
    st.setValue(QStringLiteral("imported"), imported);
    st.endGroup();
}

// --- Zugriff ----------------------------------------------------------------
int ColorSchemeRegistry::indexOf(const QString &name) const {
    for (int i = 0; i < m_schemes.size(); ++i)
        if (m_schemes.at(i).name == name) return i;
    return -1;
}

QStringList ColorSchemeRegistry::names() const {
    QStringList n;
    for (const ColorScheme &s : m_schemes) n << s.name;
    return n;
}

const ColorScheme &ColorSchemeRegistry::scheme(const QString &name) const {
    static const ColorScheme fallback;
    const int i = indexOf(name);
    if (i >= 0) return m_schemes.at(i);
    const int cur = indexOf(activeName());
    if (cur >= 0) return m_schemes.at(cur);
    return m_schemes.isEmpty() ? fallback : m_schemes.first();
}

void ColorSchemeRegistry::setDarkScheme(const QString &name) {
    if (name == m_darkChoice || indexOf(name) < 0) return;
    m_darkChoice = name;
    persist();
    emit selectionChanged();
    if (m_dark) emit changed();   // betrifft die aktuelle Ansicht
}

void ColorSchemeRegistry::setLightScheme(const QString &name) {
    if (name == m_lightChoice || indexOf(name) < 0) return;
    m_lightChoice = name;
    persist();
    emit selectionChanged();
    if (!m_dark) emit changed();
}

void ColorSchemeRegistry::setDark(bool dark) {
    if (dark == m_dark) return;
    const QString before = activeName();
    m_dark = dark;
    if (activeName() != before) emit changed();   // aktives Schema hat gewechselt
}

QVariantMap ColorSchemeRegistry::colors(const QString &name) const {
    const ColorScheme &s = scheme(name);
    QVariantList ansi;
    for (int i = 0; i < 16; ++i) ansi << hex(s.ansi[i]);
    return QVariantMap{
        { QStringLiteral("fg"), hex(s.fg) },
        { QStringLiteral("bg"), hex(s.bg) },
        { QStringLiteral("cursor"), hex(s.cursor) },
        { QStringLiteral("dark"), s.dark },
        { QStringLiteral("ansi"), ansi },
    };
}

// --- Import -----------------------------------------------------------------
namespace {

// "#rrggbb" / "rrggbb" / "0xrrggbb" -> 0xRRGGBB. Gibt false bei Unparsbarkeit.
bool parseHexColor(QString t, quint32 &out) {
    t = t.trimmed();
    if (t.startsWith(QLatin1Char('#'))) t = t.mid(1);
    else if (t.startsWith(QLatin1String("0x"))) t = t.mid(2);
    if (t.size() == 3) {  // #rgb -> #rrggbb
        t = QString(2, t.at(0)) + QString(2, t.at(1)) + QString(2, t.at(2));
    }
    if (t.size() < 6) return false;
    bool ok = false;
    out = t.left(6).toUInt(&ok, 16);
    return ok;
}

// iTerm-`.itermcolors` (XML-Plist): Keys "Ansi N Color", "Foreground/Background/Cursor Color"
// mit Float-Komponenten 0..1. Liefert true bei Erfolg.
bool parseITerm(const QByteArray &data, ColorScheme &s) {
    QXmlStreamReader xml(data);
    QString pendingKey;
    bool any = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) continue;
        if (xml.name() == QLatin1String("key")) {
            pendingKey = xml.readElementText();
        } else if (xml.name() == QLatin1String("dict") && !pendingKey.isEmpty()) {
            // Komponenten dieses Farb-Dicts einlesen.
            const QString key = pendingKey;
            pendingKey.clear();
            double r = 0, g = 0, b = 0;
            QString compKey;
            int depth = 1;
            while (!xml.atEnd() && depth > 0) {
                xml.readNext();
                if (xml.isStartElement()) {
                    if (xml.name() == QLatin1String("dict")) ++depth;
                    else if (xml.name() == QLatin1String("key")) compKey = xml.readElementText();
                    else if (xml.name() == QLatin1String("real") || xml.name() == QLatin1String("integer")) {
                        const double v = xml.readElementText().toDouble();
                        if (compKey == QLatin1String("Red Component")) r = v;
                        else if (compKey == QLatin1String("Green Component")) g = v;
                        else if (compKey == QLatin1String("Blue Component")) b = v;
                    }
                } else if (xml.isEndElement() && xml.name() == QLatin1String("dict")) {
                    --depth;
                }
            }
            auto toRgb = [](double r, double g, double b) -> quint32 {
                auto ch = [](double v) { return quint32(qBound(0.0, v, 1.0) * 255.0 + 0.5); };
                return (ch(r) << 16) | (ch(g) << 8) | ch(b);
            };
            const quint32 rgb = toRgb(r, g, b);
            bool idxOk = false;
            if (key.startsWith(QLatin1String("Ansi ")) && key.endsWith(QLatin1String(" Color"))) {
                const int idx = key.mid(5, key.size() - 5 - 6).toInt(&idxOk);
                if (idxOk && idx >= 0 && idx < 16) { s.ansi[idx] = rgb; any = true; }
            } else if (key == QLatin1String("Foreground Color")) { s.fg = rgb; any = true; }
            else if (key == QLatin1String("Background Color")) { s.bg = rgb; any = true; }
            else if (key == QLatin1String("Cursor Color")) { s.cursor = rgb; any = true; }
        }
    }
    return any && !xml.hasError();
}

// Xresources (*color0:/*.foreground:) und Ghostty (palette = 0=#…, foreground = …).
bool parseKeyValue(const QString &text, ColorScheme &s) {
    bool any = false;
    static const QRegularExpression colorN(
        QStringLiteral("(?:^|[*.])color\\s*(\\d+)\\s*[:=]\\s*(\\S+)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression palette(
        QStringLiteral("palette\\s*=\\s*(\\d+)\\s*=\\s*(\\S+)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression named(
        QStringLiteral("(?:^|[*.])(foreground|background|cursorColor|cursor-color)\\s*[:=]\\s*(\\S+)"),
        QRegularExpression::CaseInsensitiveOption);
    const QStringList lines = text.split(QChar('\n'));
    for (const QString &line : lines) {
        const QString l = line.trimmed();
        if (l.isEmpty() || l.startsWith(QChar('!')) || l.startsWith(QChar('#'))) continue;
        quint32 c = 0;
        auto m = colorN.match(l);
        if (!m.hasMatch()) m = palette.match(l);
        if (m.hasMatch()) {
            const int idx = m.captured(1).toInt();
            if (idx >= 0 && idx < 16 && parseHexColor(m.captured(2), c)) { s.ansi[idx] = c; any = true; }
            continue;
        }
        const auto mn = named.match(l);
        if (mn.hasMatch() && parseHexColor(mn.captured(2), c)) {
            const QString k = mn.captured(1).toLower();
            if (k == QLatin1String("foreground")) s.fg = c;
            else if (k == QLatin1String("background")) s.bg = c;
            else s.cursor = c;
            any = true;
        }
    }
    return any;
}

} // namespace

QString ColorSchemeRegistry::importFile(const QString &path) {
    QString p = path;
    if (p.startsWith(QLatin1String("file://"))) p = QUrl(p).toLocalFile();
    QFile f(p);
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QByteArray data = f.readAll();
    f.close();

    ColorScheme s;
    s.builtin = false;
    s.cursor = 0;  // erkennen, ob gesetzt
    const QString lower = p.toLower();
    const bool looksPlist = lower.endsWith(QLatin1String(".itermcolors"))
                            || data.trimmed().startsWith("<?xml") || data.contains("<plist");
    bool ok = looksPlist ? parseITerm(data, s) : parseKeyValue(QString::fromUtf8(data), s);
    if (!ok && !looksPlist) ok = parseITerm(data, s);  // Fallback
    if (!ok) return {};

    if (s.cursor == 0) s.cursor = s.fg;
    // Helligkeit des Hintergrunds bestimmt dark/hell (für die Vorschau).
    const int lum = ((s.bg >> 16 & 0xff) * 299 + (s.bg >> 8 & 0xff) * 587 + (s.bg & 0xff) * 114) / 1000;
    s.dark = lum < 128;

    s.name = QFileInfo(p).completeBaseName();
    if (s.name.isEmpty()) s.name = QStringLiteral("Importiert");
    // Namenskonflikt vermeiden (eingebaute nicht überschreiben, sonst Suffix).
    QString base = s.name;
    int n = 2;
    while (indexOf(s.name) >= 0) s.name = QStringLiteral("%1 (%2)").arg(base).arg(n++);

    m_schemes << s;
    // Importiertes Schema dem passenden Slot zuordnen (dunkel→Dunkel-, hell→Hell-Auswahl).
    if (s.dark) m_darkChoice = s.name; else m_lightChoice = s.name;
    persist();
    emit listChanged();
    emit selectionChanged();
    if (s.dark == m_dark) emit changed();   // betrifft die aktuelle Ansicht
    return s.name;
}

} // namespace qtmux

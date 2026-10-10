// QTMUX-142: Statusfarben als Theme-Rollen — Kontrastvertrag + Literal-Wächter.
//
// Anlass: Die Statusfarben standen als Literale der DUNKEL-Palette im QML
// (#46d369/#f5c451/#e5534b/#e0a040/#5a5d6a) und galten auch im hellen Design —
// „MCP LAN" in #f5c451 auf der Statusleiste #F2F2F2 erreichte 1,45:1.
//
// Teil 1 (contract): Jede Statusrolle hält in BEIDEN Standardschemata ihren Vertrag
// gegen die ECHTEN Flächen, auf denen sie steht — die Flächen kommen aus demselben
// Theme, das die App malt, nicht aus abgeschriebenen Hex-Werten:
//   Punkt/Rand (Statusleiste, Kacheln, Flyout, Controller-Tab) >= 3:1 gegen
//     bgSidebar, sidebarHover, sidebarSelected, bgElevated  — alle vier Rollen;
//   Text >= 4,5:1: warn gegen bgMain (Einstellungen), bgSidebar + sidebarHover
//     (MCP-Feld der Statusleiste, auch gehovert), bgElevated (Dialoge);
//     danger gegen bgElevated (Dialoge); textBright auf der 14-%-danger-Tönung
//     des Downgrade-Kastens (UpdateDialog).
// Teil 2 (oldLiteralsFailContract): eingebaute Gegenprobe — dieselbe Prüfung mit den
// alten Literalen muss im hellen Design scheitern. Ohne sie bewiese ein grüner
// Vertrag nicht, dass er überhaupt etwas messen kann.
// Teil 3 (noColorLiteralsInQml): keine Farbliterale mehr in qml/ (Hex, numerisches
// Qt.rgba/hsla …, benannte Farben außer "transparent"); Allowlist mit Begründung.
// Positivkontrollen: der Muster-Satz trifft die alten Formen und lässt Theme-Bezüge
// durch; der Scan liest echte Dateien (Main.qml dabei, Allowlist-Eintrag GEFUNDEN).

#include <QtTest>
#include <algorithm>
#include <cmath>
#include <QColor>
#include <QDirIterator>
#include <QFile>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "ColorScheme.h"
#include "Theme.h"

using qtmux::ColorSchemeRegistry;
using qtmux::Theme;

namespace {

// WCAG 2.1 relative Luminanz / Kontrastverhältnis.
double channel(double c) { return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); }
double luminance(const QColor &c)
{
    return 0.2126 * channel(c.redF()) + 0.7152 * channel(c.greenF()) + 0.0722 * channel(c.blueF());
}
double contrast(const QColor &a, const QColor &b)
{
    const double la = luminance(a), lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}
// Alpha-Überlagerung wie der Szenengraph sie malt, auf 8 Bit gerundet.
QColor over(const QColor &fg, double alpha, const QColor &bg)
{
    auto m = [&](double f, double b) { return qRound((f * alpha + b * (1 - alpha)) * 255); };
    return QColor(m(fg.redF(), bg.redF()), m(fg.greenF(), bg.greenF()), m(fg.blueF(), bg.blueF()));
}

struct Roles { QColor ok, warn, danger, muted; };

// Liefert eine Zeile je verletzter Grenze — leer = Vertrag erfüllt.
QStringList violations(const Theme &t, const Roles &r)
{
    QStringList out;
    auto need = [&](const char *what, const QColor &fg, const char *surf, const QColor &bg,
                    double min) {
        const double c = contrast(fg, bg);
        if (c < min)
            out << QStringLiteral("%1 %2 auf %3 %4: %5 < %6")
                       .arg(QLatin1String(what), fg.name(), QLatin1String(surf), bg.name())
                       .arg(c, 0, 'f', 2).arg(min, 0, 'f', 1);
    };
    const struct { const char *name; QColor c; } roles[] = {
        {"ok", r.ok}, {"warn", r.warn}, {"danger", r.danger}, {"muted", r.muted}};
    for (const auto &role : roles) {
        need(role.name, role.c, "bgSidebar", t.bgSidebar(), 3.0);
        need(role.name, role.c, "sidebarHover", t.sidebarHover(), 3.0);
        need(role.name, role.c, "sidebarSelected", t.sidebarSelected(), 3.0);
        need(role.name, role.c, "bgElevated", t.bgElevated(), 3.0);
    }
    need("warn-Text", r.warn, "bgMain", t.bgMain(), 4.5);
    need("warn-Text", r.warn, "bgSidebar", t.bgSidebar(), 4.5);
    need("warn-Text", r.warn, "sidebarHover", t.sidebarHover(), 4.5);
    need("warn-Text", r.warn, "bgElevated", t.bgElevated(), 4.5);
    need("danger-Text", r.danger, "bgElevated", t.bgElevated(), 4.5);
    need("textBright", t.textBright(), "danger-Toenung", over(r.danger, 0.14, t.bgElevated()), 4.5);
    return out;
}

Roles themeRoles(const Theme &t) { return {t.ok(), t.warn(), t.danger(), t.muted()}; }

// Ein Farbliteral im QML-Quelltext: Hex in Anführungszeichen, numerisches
// Qt.rgba/rgb/hsla/hsva/hsl/hsv, benannte Farbe an einer color-Eigenschaft.
const QRegularExpression &literalPattern()
{
    static const QRegularExpression re(QStringLiteral(
        R"re("#[0-9A-Fa-f]{3,8}"|Qt\.(?:rgba|rgb|hsla|hsva|hsl|hsv)\(\s*[-0-9.]|[cC]olor\s*:\s*"(?!transparent")[A-Za-z]+")re"));
    return re;
}

// Allowlist: Datei (relativ zu qml/) -> erlaubtes Literal. Jede Zeile braucht einen
// Grund, und jeder Eintrag MUSS beim Scan gefunden werden (sonst ist er tot).
const QList<QPair<QString, QString>> kAllowed = {
    // Modale Abdunklung hinter Dialogen: Schwarz mit 53 % — in beiden Designs gleich
    // gewollt, keine Status- und keine Chrome-Farbe.
    {QStringLiteral("Ui/AppDialog.qml"), QStringLiteral("\"#88000000\"")},
};

} // namespace

class tst_statuscolors : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void contract_data();
    void contract();
    void oldLiteralsFailContract();
    void patternPositiveControl();
    void noColorLiteralsInQml();

private:
    QTemporaryDir m_dir;
};

void tst_statuscolors::initTestCase()
{
    QVERIFY(m_dir.isValid());
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_dir.path());
    QCoreApplication::setOrganizationName(QStringLiteral("QTmuxTest"));
    QCoreApplication::setApplicationName(QStringLiteral("tst_statuscolors"));
    // Gemessen wird gegen die Standardschemata — dieselben, die die App ohne
    // Einstellungen wählt.
    QCOMPARE(ColorSchemeRegistry::instance()->darkScheme(), QStringLiteral("QTmux Dunkel"));
    QCOMPARE(ColorSchemeRegistry::instance()->lightScheme(), QStringLiteral("QTmux Hell"));
}

void tst_statuscolors::contract_data()
{
    QTest::addColumn<int>("mode");
    QTest::addColumn<bool>("expectDark");
    QTest::newRow("hell") << int(Theme::Light) << false;
    QTest::newRow("dunkel") << int(Theme::Dark) << true;
}

void tst_statuscolors::contract()
{
    QFETCH(int, mode);
    QFETCH(bool, expectDark);
    Theme t;
    t.setMode(Theme::Mode(mode));
    // Objekt-Bindung: gemessen wird wirklich das gewünschte Design.
    QCOMPARE(t.dark(), expectDark);
    QCOMPARE(t.statusDark(), expectDark);
    const QStringList v = violations(t, themeRoles(t));
    if (!v.isEmpty())
        QFAIL(qPrintable(v.join(QLatin1Char('\n'))));
}

void tst_statuscolors::oldLiteralsFailContract()
{
    Theme t;
    t.setMode(Theme::Light);
    const Roles old{QColor(0x46d369), QColor(0xf5c451), QColor(0xe5534b), QColor(0x5a5d6a)};
    const QStringList v = violations(t, old);
    // Die vier Befunde aus der Design-Inventur müssen auftauchen.
    auto has = [&](const QString &s) {
        for (const QString &line : v)
            if (line.startsWith(s)) return true;
        return false;
    };
    QVERIFY2(has(QStringLiteral("warn-Text #f5c451 auf bgSidebar #f2f2f2: 1.45")),
             qPrintable(v.join(QLatin1Char('\n'))));
    QVERIFY(has(QStringLiteral("ok #46d369 auf bgSidebar")));
    QVERIFY(has(QStringLiteral("danger-Text #e5534b auf bgElevated")));
    // Kontrollwert unabhängig vom Theme: Schwarz/Weiß = 21:1.
    QCOMPARE(qRound(contrast(Qt::black, Qt::white) * 100), 2100);
}

void tst_statuscolors::patternPositiveControl()
{
    const QRegularExpression &re = literalPattern();
    const QStringList mustHit = {
        QStringLiteral("color: \"#f5c451\""),
        QStringLiteral(": st === 1 ? \"#46d369\" : x"),
        QStringLiteral("border.color: \"#E5534B\""),
        QStringLiteral("color: Qt.rgba(0.9, 0.33, 0.29, 0.14)"),
        QStringLiteral("color: Qt.hsla(0.1, 1, 0.5, 1)"),
        QStringLiteral("color: \"orange\""),
        QStringLiteral("dotColor: \"red\""),
    };
    for (const QString &s : mustHit)
        QVERIFY2(re.match(s).hasMatch(), qPrintable(s));
    const QStringList mustPass = {
        QStringLiteral("color: Theme.warn"),
        QStringLiteral("color: Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.14)"),
        QStringLiteral("color: \"transparent\""),
        QStringLiteral("text: \"#\" + actSid"),
    };
    for (const QString &s : mustPass)
        QVERIFY2(!re.match(s).hasMatch(), qPrintable(s));
}

void tst_statuscolors::noColorLiteralsInQml()
{
    const QString root = QStringLiteral(QTMUX_SOURCE_DIR "/qml");
    QDirIterator it(root, {QStringLiteral("*.qml")}, QDir::Files, QDirIterator::Subdirectories);
    int files = 0;
    bool sawMain = false;
    QSet<QString> allowedSeen;
    QStringList hits;
    while (it.hasNext()) {
        const QString path = it.next();
        const QString rel = QDir(root).relativeFilePath(path);
        QFile f(path);
        QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(path));
        const QString text = QString::fromUtf8(f.readAll());
        ++files;
        if (rel == QLatin1String("Main.qml") && text.size() > 100000) sawMain = true;
        auto m = literalPattern().globalMatch(text);
        while (m.hasNext()) {
            const auto hit = m.next();
            const QString lit = hit.captured(0);
            bool allowed = false;
            for (const auto &a : kAllowed)
                if (a.first == rel && a.second == lit) { allowed = true; allowedSeen.insert(rel + lit); }
            if (!allowed) {
                const int line = int(text.left(hit.capturedStart()).count(QLatin1Char('\n'))) + 1;
                hits << QStringLiteral("%1:%2: %3").arg(rel).arg(line).arg(lit);
            }
        }
    }
    // Positivkontrollen: echte Dateien gelesen, Allowlist-Einträge tatsächlich getroffen.
    QVERIFY2(files >= 20, qPrintable(QString::number(files)));
    QVERIFY(sawMain);
    for (const auto &a : kAllowed)
        QVERIFY2(allowedSeen.contains(a.first + a.second),
                 qPrintable(QStringLiteral("Allowlist-Eintrag nicht gefunden (tot?): %1 %2")
                                .arg(a.first, a.second)));
    if (!hits.isEmpty())
        QFAIL(qPrintable(QStringLiteral("Farbliterale in qml/ — Theme-Rolle benutzen "
                                        "(Statusfarben: Theme.ok/warn/danger/muted):\n")
                         + hits.join(QLatin1Char('\n'))));
}

QTEST_MAIN(tst_statuscolors)
#include "tst_statuscolors.moc"

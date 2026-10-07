// QTMUX-141: Wächter gegen fehlende Icon-Ressourcen.
//
// Anlass: Seit 1.9.6 (QTMUX-138) verwies „Im Finder zeigen" auf qrc:/icons/folder.svg,
// das nie eingebettet war — Menüeintrag ohne Icon, Startwarnung „IconImage: Cannot open".
// Ein wörtliches grep nach „folder.svg" fand nichts, weil der Pfad in Main.qml erst in
// icon(name) zusammengesetzt wird. Genau diese Klasse prüft der Test: Er zieht ALLE
// Icon-Namen aus dem Quelltext — auch die zusammengesetzten — und hält sie gegen die
// Ressourcen, die tests/CMakeLists.txt aus derselben Liste einbettet wie die App
// (QTMUX_ICON_FILES im Haupt-CMakeLists).
//
// Erfasste Verweis-Wege (je Weg eine Positivkontrolle in extractionCoversAllPaths):
//   call     icon("x") / iconSrc("x") — auch Ternaries: icon(c ? "a" : "b")
//   field    Datenfelder  icon: "x"  (Palette, Einstellungs-Kategorien)
//   function String-Literale in Icon-Namensfunktionen (function profileIcon(…) { … })
//   literal  wörtliche Pfade qrc:/icons/x.svg bzw. :/icons/x.svg (QML und C++)
// Ein icon(…)-Argument bzw. icon:-Feld, das sich NICHT auf diese Wege zurückführen lässt
// (z. B. icon(someVar)), schlägt als „unerklärte dynamische Icon-Quelle" an — sonst
// könnte eine neue Namensquelle still am Wächter vorbeilaufen.

#include <QtTest>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QSet>

namespace {

struct IconRef {
    QString name;
    QString path;   // Erfassungsweg: call/field/function/literal
    QString where;  // Datei:Zeile
};

QString readText(const QString &file)
{
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(f.readAll());
}

int lineAt(const QString &text, qsizetype pos)
{
    return int(text.left(pos).count(QLatin1Char('\n'))) + 1;
}

// Index der schließenden Klammer zu text[open] (open zeigt auf die öffnende), -1 ohne
// Gegenstück. String-Literale werden übersprungen.
qsizetype matchingClose(const QString &text, qsizetype open, QChar o, QChar c)
{
    int depth = 0;
    for (qsizetype i = open; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        if (ch == QLatin1Char('"')) {
            for (++i; i < text.size() && text.at(i) != QLatin1Char('"'); ++i)
                if (text.at(i) == QLatin1Char('\\')) ++i;
            continue;
        }
        if (ch == o) ++depth;
        else if (ch == c && --depth == 0) return i;
    }
    return -1;
}

QStringList literalsIn(const QString &expr)
{
    static const QRegularExpression re(QStringLiteral("\"([^\"\\\\]*)\""));
    QStringList out;
    for (auto it = re.globalMatch(expr); it.hasNext();)
        out << it.next().captured(1);
    return out;
}

} // namespace

class TstIcons : public QObject
{
    Q_OBJECT

    QList<IconRef> m_refs;
    QStringList m_unexplained;
    QStringList m_iconFunctions;
    int m_qmlFiles = 0;

    // Ist ein Ausdruck (icon(…)-Argument bzw. Wert eines icon:-Feldes) vollständig auf
    // erfasste Wege zurückführbar? Literale, Aufrufe von Icon-Namensfunktionen und
    // „….icon"-Member (gespeist aus icon:-Feldern) werden herausgenommen; übrig bleiben
    // dürfen nur Bedingungen von Ternaries, nie ein Zweig.
    bool explained(QString expr) const
    {
        expr.replace(QRegularExpression(QStringLiteral("\"([^\"\\\\]*)\"")), QString());
        for (const QString &fn : m_iconFunctions) {
            const QRegularExpression call(QStringLiteral("[\\w.]*\\b%1\\s*\\(").arg(fn));
            for (auto m = call.match(expr); m.hasMatch(); m = call.match(expr)) {
                const qsizetype open = m.capturedEnd() - 1;
                const qsizetype close = matchingClose(expr, open, u'(', u')');
                if (close < 0) return false;
                expr.remove(m.capturedStart(), close - m.capturedStart() + 1);
            }
        }
        expr.replace(QRegularExpression(QStringLiteral(R"re([\w.\[\]]*\.icon\b)re")), QString());

        // Segmente zwischen ? und : — vor einem „?" steht eine Bedingung (darf alles),
        // jedes andere Segment ist ein Zweig und muss leer geworden sein.
        static const QRegularExpression delim(QStringLiteral("[?:]"));
        qsizetype start = 0;
        for (;;) {
            const auto m = delim.match(expr, start);
            const qsizetype end = m.hasMatch() ? m.capturedStart() : expr.size();
            const bool isCondition = m.hasMatch() && m.captured() == QLatin1String("?");
            if (!isCondition && !expr.mid(start, end - start).trimmed().isEmpty())
                return false;
            if (!m.hasMatch()) return true;
            start = m.capturedEnd();
        }
    }

    void addRef(const QString &name, const QString &path, const QString &where)
    {
        m_refs.append({name, path, where});
    }

    void scanFunctions(const QString &text, const QString &rel)
    {
        static const QRegularExpression re(QStringLiteral(R"re(\bfunction\s+(\w*Icon\w*)\s*\()re"));
        for (auto it = re.globalMatch(text); it.hasNext();) {
            const auto m = it.next();
            const qsizetype brace = text.indexOf(QLatin1Char('{'), m.capturedEnd());
            const qsizetype close = brace < 0 ? -1 : matchingClose(text, brace, u'{', u'}');
            if (close < 0) {
                m_unexplained << QStringLiteral("%1:%2 Icon-Funktion %3 ohne Rumpf")
                                     .arg(rel).arg(lineAt(text, m.capturedStart())).arg(m.captured(1));
                continue;
            }
            const QString where = QStringLiteral("%1:%2").arg(rel).arg(lineAt(text, m.capturedStart()));
            for (const QString &lit : literalsIn(text.mid(brace, close - brace + 1)))
                addRef(lit, QStringLiteral("function"), where);
        }
    }

    void scanCalls(const QString &text, const QString &rel)
    {
        static const QRegularExpression re(QStringLiteral(R"re((\bfunction\s+)?\b(icon|iconSrc)\s*\()re"));
        for (auto it = re.globalMatch(text); it.hasNext();) {
            const auto m = it.next();
            if (!m.captured(1).isEmpty())
                continue;   // die Definition selbst, kein Verweis
            const qsizetype open = m.capturedEnd() - 1;
            const qsizetype close = matchingClose(text, open, u'(', u')');
            const QString where = QStringLiteral("%1:%2").arg(rel).arg(lineAt(text, m.capturedStart()));
            const QString arg = close < 0 ? QString() : text.mid(open + 1, close - open - 1);
            for (const QString &lit : literalsIn(arg))
                addRef(lit, QStringLiteral("call"), where);
            if (close < 0 || !explained(arg))
                m_unexplained << QStringLiteral("%1 %2(%3)").arg(where, m.captured(2), arg.simplified());
        }
    }

    void scanFields(const QString &text, const QString &rel)
    {
        // Wert bis zum nächsten „," / „}" / Zeilenende auf Klammertiefe 0.
        static const QRegularExpression re(QStringLiteral(R"re(\bicon\s*:\s*)re"));
        for (auto it = re.globalMatch(text); it.hasNext();) {
            const auto m = it.next();
            qsizetype i = m.capturedEnd();
            int depth = 0;
            for (; i < text.size(); ++i) {
                const QChar ch = text.at(i);
                if (ch == QLatin1Char('"')) {
                    for (++i; i < text.size() && text.at(i) != QLatin1Char('"'); ++i)
                        if (text.at(i) == QLatin1Char('\\')) ++i;
                    continue;
                }
                if (ch == u'(' || ch == u'[') ++depth;
                else if (ch == u')' || ch == u']') --depth;
                else if (depth <= 0 && (ch == u',' || ch == u'}' || ch == u'\n' || ch == u';')) break;
            }
            const QString value = text.mid(m.capturedEnd(), i - m.capturedEnd());
            const QString where = QStringLiteral("%1:%2").arg(rel).arg(lineAt(text, m.capturedStart()));
            for (const QString &lit : literalsIn(value))
                addRef(lit, QStringLiteral("field"), where);
            if (!explained(value))
                m_unexplained << QStringLiteral("%1 icon: %2").arg(where, value.simplified());
        }
    }

    void scanLiterals(const QString &text, const QString &rel)
    {
        static const QRegularExpression re(QStringLiteral(R"re((?:qrc)?:/icons/([\w-]+)\.svg)re"));
        for (auto it = re.globalMatch(text); it.hasNext();) {
            const auto m = it.next();
            addRef(m.captured(1), QStringLiteral("literal"),
                   QStringLiteral("%1:%2").arg(rel).arg(lineAt(text, m.capturedStart())));
        }
    }

private slots:
    void initTestCase()
    {
        const QDir root(QStringLiteral(QTMUX_SOURCE_DIR));
        QVERIFY2(root.exists(QStringLiteral("qml/Main.qml")),
                 qPrintable(QStringLiteral("Quellbaum nicht gefunden: ") + root.path()));

        QStringList qml, cpp;
        for (QDirIterator it(root.filePath(QStringLiteral("qml")), {QStringLiteral("*.qml")},
                             QDir::Files, QDirIterator::Subdirectories); it.hasNext();)
            qml << it.next();
        for (QDirIterator it(root.filePath(QStringLiteral("src")),
                             {QStringLiteral("*.cpp"), QStringLiteral("*.h")},
                             QDir::Files, QDirIterator::Subdirectories); it.hasNext();)
            cpp << it.next();
        m_qmlFiles = int(qml.size());

        // Erst alle Icon-Namensfunktionen sammeln — ein Aufruf in Datei A kann eine
        // Funktion aus Datei B meinen (CatVerbindungen ruft Main.qmls profileIcon).
        static const QRegularExpression fnName(QStringLiteral(R"re(\bfunction\s+(\w*Icon\w*)\s*\()re"));
        for (const QString &f : qml)
            for (auto it = fnName.globalMatch(readText(f)); it.hasNext();)
                m_iconFunctions << it.next().captured(1);
        m_iconFunctions.removeDuplicates();

        for (const QString &f : qml) {
            const QString text = readText(f), rel = root.relativeFilePath(f);
            scanFunctions(text, rel);
            scanCalls(text, rel);
            scanFields(text, rel);
            scanLiterals(text, rel);
        }
        for (const QString &f : cpp)
            scanLiterals(readText(f), root.relativeFilePath(f));
    }

    // Positivkontrolle: Jeder Erfassungsweg findet nachweislich etwas. Ohne sie wäre ein
    // kaputter Regex ein stiller grüner Test (nichts gefunden = nichts fehlt).
    void extractionCoversAllPaths()
    {
        QVERIFY2(m_qmlFiles >= 10, qPrintable(QString::number(m_qmlFiles)));
        QVERIFY(m_iconFunctions.contains(QStringLiteral("profileIcon")));
        QVERIFY(m_iconFunctions.contains(QStringLiteral("sidebarIconFor")));

        auto has = [this](const QString &name, const QString &path) {
            for (const IconRef &r : m_refs)
                if (r.name == name && r.path == path) return true;
            return false;
        };
        QVERIFY2(has(QStringLiteral("folder"), QStringLiteral("call")), "icon(\"folder\") nicht erfasst");
        QVERIFY2(has(QStringLiteral("sun"), QStringLiteral("call")), "Ternary-Aufruf nicht erfasst");
        QVERIFY2(has(QStringLiteral("translate"), QStringLiteral("field")), "icon:-Feld nicht erfasst");
        QVERIFY2(has(QStringLiteral("usb"), QStringLiteral("function")), "profileIcon nicht erfasst");
        QVERIFY2(has(QStringLiteral("caret-down"), QStringLiteral("literal")), "qrc-Literal nicht erfasst");

        QSet<QString> names;
        for (const IconRef &r : m_refs) names.insert(r.name);
        QVERIFY2(names.size() >= 20, qPrintable(QString::number(names.size())));
    }

    // Kern: jeder verwendete Name liegt als :/icons/<name>.svg im Binary.
    void everyReferencedIconIsEmbedded()
    {
        QVERIFY2(QDir(QStringLiteral(":/icons")).entryList().size() >= 20,
                 "Icon-Ressourcen nicht eingebettet — tests/CMakeLists.txt prüfen");
        QStringList missing;
        for (const IconRef &r : m_refs)
            if (!QFile::exists(QStringLiteral(":/icons/%1.svg").arg(r.name)))
                missing << QStringLiteral("%1 [%2] %3").arg(r.where, r.path, r.name);
        QVERIFY2(missing.isEmpty(),
                 qPrintable(QStringLiteral("Icon fehlt in QTMUX_ICON_FILES bzw. resources/icons/:\n  ")
                            + missing.join(QStringLiteral("\n  "))));
    }

    void noUnexplainedDynamicIconSource()
    {
        QVERIFY2(m_unexplained.isEmpty(),
                 qPrintable(QStringLiteral("Icon-Quelle, die der Wächter nicht auflösen kann — "
                                           "Namen literal machen oder tst_icons erweitern:\n  ")
                            + m_unexplained.join(QStringLiteral("\n  "))));
    }

    // Gegenrichtung der Liste: eine SVG, die in resources/icons/ liegt, aber in
    // QTMUX_ICON_FILES fehlt, ist der zweite Weg zum selben Fehlerbild.
    void everyIconFileOnDiskIsEmbedded()
    {
        const QDir icons(QStringLiteral(QTMUX_SOURCE_DIR "/resources/icons"));
        const QStringList onDisk = icons.entryList({QStringLiteral("*.svg")}, QDir::Files);
        QVERIFY(!onDisk.isEmpty());
        QStringList notEmbedded;
        for (const QString &f : onDisk)
            if (!QFile::exists(QStringLiteral(":/icons/") + f))
                notEmbedded << f;
        QVERIFY2(notEmbedded.isEmpty(), qPrintable(notEmbedded.join(QStringLiteral(", "))));

        // Nur zur Information: eingebettet, aber nirgends verwendet.
        QSet<QString> used;
        for (const IconRef &r : m_refs) used.insert(r.name + QStringLiteral(".svg"));
        for (const QString &f : QDir(QStringLiteral(":/icons")).entryList())
            if (!used.contains(f))
                qInfo("unbenutztes Icon: %s", qPrintable(f));
    }
};

QTEST_GUILESS_MAIN(TstIcons)
#include "tst_icons.moc"

// Kommen die Update-Texte des Hubs im QTmux-Bau an? (Vertrag V2 §4.5, K2)
//
// Der Hub liefert seine Kataloge (third_party/updater/update/i18n/*.ts) selbst
// mit; QTmux bettet sie über appupdate_add_catalogs(qtmux_updater) als Ressource
// der statischen Lib ein. Fehlt dieser Aufruf, gibt es KEINEN Baufehler: Die Texte
// bleiben still englisch, und installUpdateTranslator() liefert false (so im Hub
// gemessen, Kommentar in UpdaterCatalogs.cmake). Dieser Test ist der Wächter dafür
// an der Vendoring-Bauart, die der Hub selbst nicht hat. Mutationsprobe K2: Ohne
// den Aufruf in CMakeLists.txt fielen alle vier Fälle.

#include <QCoreApplication>
#include <QHash>
#include <QRegularExpression>
#include <QTest>

#include "update/UpdateTexts.hpp"

namespace {

// Das Muster aus dem früheren tst_updateviewmodel (QTMUX-146), dazu die schwedischen
// Wörter: Seit macOS 15 gibt Rechtsklick › Öffnen die Gatekeeper-Quarantäne nicht
// mehr frei; kein Satz darf dazu raten.
const QRegularExpression kRightClickAdvice(
    QStringLiteral("right[- ]?click|rechte[nr]? Maustaste|Rechtsklick|context menu|"
                   "Kontextmen|choose “Open”|choose \"Open\"|„Öffnen“|"
                   "högerklick|snabbmeny"),
    QRegularExpression::CaseInsensitiveOption);

QHash<QString, QString> leadArgs()
{
    return {{QStringLiteral("neu"), QStringLiteral("1.10.0")},
            {QStringLiteral("alt"), QStringLiteral("1.9.15")}};
}

} // namespace

class TestUpdaterCatalogs : public QObject {
    Q_OBJECT

private slots:
    void germanCatalogueIsEmbedded()
    {
        QVERIFY2(appupdate::installUpdateTranslator(QStringLiteral("de")),
                 "Hub-Katalog de fehlt: appupdate_add_catalogs(qtmux_updater) nicht gelaufen?");
        const appupdate::UpdateTexts de(QStringLiteral("QTmux"), QStringLiteral("de"));
        QCOMPARE(de.get("s3.lead", leadArgs()),
                 QStringLiteral("QTmux 1.10.0 ist verfügbar. Installiert ist 1.9.15."));
    }

    void almostEveryGermanSentenceIsTranslated()
    {
        // Ein leerer, halber oder falscher Katalog lieferte den englischen Quelltext.
        // Gemessen am Hub-Stand e463725: 146 von 154 Sätzen weichen ab, die übrigen
        // 8 sind im Deutschen gleich. Mehr als 10 gleiche Sätze heißt: Es fehlen
        // Übersetzungen.
        const QStringList keys = appupdate::UpdateTexts::keys();
        QVERIFY(keys.size() > 100);
        QVERIFY(appupdate::installUpdateTranslator(QStringLiteral("en")));
        const appupdate::UpdateTexts en(QStringLiteral("QTmux"), QStringLiteral("en"));
        QStringList english;
        for (const QString& k : keys) english << en.get(k.toUtf8().constData());
        QVERIFY(appupdate::installUpdateTranslator(QStringLiteral("de")));
        const appupdate::UpdateTexts de(QStringLiteral("QTmux"), QStringLiteral("de"));
        QStringList same;
        for (int i = 0; i < keys.size(); ++i) {
            if (de.get(keys.at(i).toUtf8().constData()) == english.at(i)) same << keys.at(i);
        }
        QVERIFY2(same.size() <= 10,
                 qPrintable(QStringLiteral("%1 von %2 Sätzen unübersetzt: %3")
                                .arg(same.size())
                                .arg(keys.size())
                                .arg(same.join(QLatin1String(", ")))));
    }

    void noSentenceAdvisesRightClickOpen()
    {
        // Prüft den Hub-Wortlaut in allen drei Sprachen, die der Hub mitbringt.
        // Seit K2 kommen die Sätze des Update-Dialogs, der Proxy-Anmeldung und der
        // Seite „Updates“ von dort; die eigenen Update-Texte (bisher von
        // tst_updateviewmodel geprüft) gibt es nicht mehr.
        const QStringList keys = appupdate::UpdateTexts::keys();
        for (const QString lang :
             {QStringLiteral("de"), QStringLiteral("en"), QStringLiteral("sv")}) {
            QVERIFY(appupdate::installUpdateTranslator(lang));
            const appupdate::UpdateTexts t(QStringLiteral("QTmux"), lang);
            for (const QString& k : keys) {
                const QString s = t.get(k.toUtf8().constData());
                QVERIFY2(!kRightClickAdvice.match(s).hasMatch(),
                         qPrintable(lang + QLatin1Char(' ') + k + QLatin1String(": ") + s));
            }
        }
    }

    void swedishCatalogueIsEmbedded()
    {
        QVERIFY(appupdate::installUpdateTranslator(QStringLiteral("sv")));
        const appupdate::UpdateTexts sv(QStringLiteral("QTmux"), QStringLiteral("sv"));
        QCOMPARE(sv.get("s3.lead", leadArgs()),
                 QStringLiteral("QTmux 1.10.0 är tillgänglig. Du har 1.9.15."));
    }
};

QTEST_GUILESS_MAIN(TestUpdaterCatalogs)
#include "tst_updatercatalogs.moc"

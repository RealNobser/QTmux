#pragma once

// appupdate::UpdateTexts — ONE wording for both surfaces and all four apps
// (update contract V2, 2026-11-19, docs/SHARED.md).
//
// CANONICAL: QTmux vendors src/update/ byte-identically. Qt6::Core only.
//
// Every sentence lives in the table in UpdateTexts.cpp, context
// "UpdateTexts", with the design's key (e.g. "s3.lead") as disambiguation, so
// a surface asks by key and never owns a sentence. Source language English;
// catalogues de (Sie) and sv in src/update/i18n/, embedded in the lib and
// installed with installUpdateTranslator() — the app calls it once with its
// UI language, next to its own translators.
//
// Placeholders, as in the design: {product} ([Product] in the design), {neu}
// offered version, {alt} installed version, {datum} release date (long,
// by language), {groesse} size with unit, {geladen} amount without unit,
// {prozent} whole number, {n} count, {system} system in words, {proxy}
// proxy host, {kennung} full build id.
//
// Keys starting with "x." are NOT in the design: sentences the flow needs and
// the design does not draw (working state, not yet reviewed).

#include <QDate>
#include <QHash>
#include <QLocale>
#include <QString>
#include <QStringList>

namespace appupdate {

// Loads the hub catalogue for `language` ("de", "sv"; "en" and unknown
// languages need none and return true) from :/appupdate/i18n/ and installs
// it on the application. Idempotent per language; replaces a previously
// installed hub catalogue of another language. Returns false when the
// catalogue is missing — that is a build defect (the resource was not linked,
// see src/update/UpdaterCatalogs.cmake), and the texts stay English.
bool installUpdateTranslator(const QString& language);

class UpdateTexts {
public:
    UpdateTexts(QString productName, QString language);

    [[nodiscard]] const QString& productName() const { return m_product; }
    [[nodiscard]] const QString& language() const { return m_language; }
    [[nodiscard]] QLocale locale() const { return m_locale; }

    // The translated sentence for `key` with {product} and `args` filled in.
    // An unknown key returns the key itself in brackets — visible in every
    // picture, never an empty label.
    [[nodiscard]] QString get(const char* key,
                              const QHash<QString, QString>& args = {}) const;

    // True when the table knows `key`.
    [[nodiscard]] static bool hasKey(const char* key);
    // All keys, in table order (tests, catalogue checks).
    [[nodiscard]] static QStringList keys();

    // ---- Numbers, dates, systems (the design) -------------------------------
    // "98,4 MB" / "98.4 MB": SI, one decimal.
    [[nodiscard]] QString size(qint64 bytes) const;
    // "38,2" — `bytes` in the unit of `totalBytes`, without unit.
    [[nodiscard]] QString amountInUnitOf(qint64 bytes, qint64 totalBytes) const;
    // "18. November 2026" / "November 18, 2026"; never ISO.
    [[nodiscard]] QString longDate(const QDate& date) const;
    // An ISO date or date-time string from the manifest → longDate, or empty.
    [[nodiscard]] QString longDateFromIso(const QString& iso) const;
    // "39 %" (de, U+202F) / "39%" (en) — via the catalogue key s4.percent.
    [[nodiscard]] QString percent(int value) const;
    // "macOS", "Windows (x86, 64 Bit)", "Linux (x86, 64 Bit)" … for a
    // manifest artifact key; never the key itself.
    [[nodiscard]] QString systemName(const QString& artifactKey) const;

private:
    QString m_product;
    QString m_language;
    QLocale m_locale;
};

}  // namespace appupdate

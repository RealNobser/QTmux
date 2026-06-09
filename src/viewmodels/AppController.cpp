#include "AppController.h"
#include <QSettings>
#include <QLocale>
#include <QFontDatabase>

namespace qtmux {

AppController::AppController(QObject *parent) : QObject(parent) {
    QSettings s;
    // Default: System-Sprache, sofern unterstützt, sonst Englisch.
    const QString sys = QLocale::system().name().left(2);
    const QString fallback = (sys == QLatin1String("de")) ? QStringLiteral("de") : QStringLiteral("en");
    m_language = s.value(QStringLiteral("ui/language"), fallback).toString();
}

void AppController::setLanguage(const QString &lang) {
    if (lang == m_language || !languageCodes().contains(lang)) return;
    m_language = lang;
    QSettings().setValue(QStringLiteral("ui/language"), lang);
    emit languageChanged(lang);
}

QString AppController::languageName(const QString &code) const {
    if (code == QLatin1String("de")) return QStringLiteral("Deutsch");
    if (code == QLatin1String("en")) return QStringLiteral("English");
    return code;
}

QStringList AppController::monospaceFonts() const {
    QStringList out;
    const QStringList all = QFontDatabase::families(QFontDatabase::Latin);
    for (const QString &fam : all) {
        if (fam.startsWith(QLatin1Char('.'))) continue;   // versteckte System-Fonts
        if (QFontDatabase::isFixedPitch(fam)) out << fam;
    }
    // Standard-Familie sicher enthalten (manche melden isFixedPitch nicht zuverlässig).
    const QString def = defaultMonospaceFont();
    if (!def.isEmpty() && !out.contains(def)) out.prepend(def);
    out.removeDuplicates();
    return out;
}

QString AppController::defaultMonospaceFont() const {
    return QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
}

} // namespace qtmux

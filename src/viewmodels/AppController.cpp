#include "AppController.h"
#include <QSettings>
#include <QLocale>

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

} // namespace qtmux

#pragma once

#include <QObject>
#include <QString>
#include <qqmlintegration.h>

namespace qtmux {

/// Hält die UI-Sprache (persistiert via QSettings) als QML-Singleton (`App.*`).
/// Das eigentliche Laden des QTranslators + engine->retranslate() passiert in main.cpp,
/// angestoßen über das Signal `languageChanged` (die QML-Engine ist dort verfügbar).
class AppController : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(App)
    QML_SINGLETON
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
public:
    explicit AppController(QObject *parent = nullptr);

    QString language() const { return m_language; }
    void setLanguage(const QString &lang);

    /// Unterstützte Sprachen (Code -> Anzeigename) für das Sprachmenü.
    Q_INVOKABLE QStringList languageCodes() const { return {QStringLiteral("de"), QStringLiteral("en")}; }
    Q_INVOKABLE QString languageName(const QString &code) const;

signals:
    void languageChanged(const QString &lang);

private:
    QString m_language;
};

} // namespace qtmux

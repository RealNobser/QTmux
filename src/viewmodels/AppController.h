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

    /// Wandelt ein QML-Tasten-Event (Qt::Key + Qt::KeyboardModifiers als int) in einen
    /// kanonischen Akkord-String im Portable-Format um (z. B. "Ctrl+Shift+E"). Reine
    /// Modifier-Tasten liefern "" (für die Hotkey-Aufnahme, QTMUX-15). Nutzt QKeySequence
    /// (QtGui) — hier korrekt, da AppController im Gui-Target lebt; die HotkeyRegistry
    /// bleibt dadurch Gui-frei.
    Q_INVOKABLE QString keyChord(int key, int modifiers) const;

    /// Kopiert Text in die System-Zwischenablage (z. B. ein Vault-Geheimnis).
    Q_INVOKABLE void copyToClipboard(const QString &text) const;

    /// Installierte Monospace-Schriftfamilien (für die Terminal-Schriftwahl).
    Q_INVOKABLE QStringList monospaceFonts() const;
    /// Plattformübliche Standard-Monospace-Familie (Default der Terminal-Schrift).
    Q_INVOKABLE QString defaultMonospaceFont() const;

signals:
    void languageChanged(const QString &lang);

private:
    QString m_language;
};

} // namespace qtmux

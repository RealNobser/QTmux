#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
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

    /// Build-ID dieses Programms: `<version>+<git-short-hash>[-dirty]`.
    /// Steht im **Fenstertitel**, nicht nur im Über-Dialog — man muss einer
    /// laufenden Anwendung ansehen, ob sie der frisch gebaute Stand ist.
    /// ⚠️ Reine **Anzeige**. Der Versionsvergleich gegen das Update-Manifest
    /// läuft über `Updates.currentVersion` und bekommt nur „1.8.0" — mit
    /// angehängtem Hash schlüge er fehl.
    Q_PROPERTY(QString buildId READ buildId CONSTANT)
    /// Wurde aus einem Baum mit uncommitteten Änderungen gebaut? Treibt die
    /// Kennzeichnung in der Oberfläche.
    Q_PROPERTY(bool buildIsDirty READ buildIsDirty CONSTANT)
    // „Bewegung reduzieren" des Systems (QTMUX-47, Teil B): true → die Sidebar zeichnet
    // pulsierende Status-Elemente statisch statt animiert. Beim Start ermittelt (macOS
    // universalaccess reduceMotion, Windows SPI_GETCLIENTAREAANIMATION), sonst false;
    // Umgebungsvariable QTMUX_REDUCE_MOTION erzwingt den Wert (für Tests). Read-only.
    Q_PROPERTY(bool reduceMotion READ reduceMotion NOTIFY reduceMotionChanged)
    // Home-Verzeichnis des Benutzers, damit die Sidebar Pfade als `~/…` kürzen kann
    // (Anzeige-Logik bleibt in QML, wie bei QTMUX-103). Konstant: ein Wechsel des
    // Home-Verzeichnisses zur Laufzeit ist kein Fall, den QTmux behandeln muss.
    Q_PROPERTY(QString homeDir READ homeDir CONSTANT)
public:
    explicit AppController(QObject *parent = nullptr);

    QString language() const { return m_language; }
    void setLanguage(const QString &lang);

    QString buildId() const;
    bool buildIsDirty() const;

    bool reduceMotion() const { return m_reduceMotion; }

    QString homeDir() const;

    /// Unterstützte Sprachen (Code -> Anzeigename) für das Sprachmenü.
    Q_INVOKABLE QStringList languageCodes() const { return {QStringLiteral("de"), QStringLiteral("en")}; }
    Q_INVOKABLE QString languageName(const QString &code) const;

    /// Liest `ui/language` erneut (nach Reset/Import, Stufe 6). Wie Theme::reload()
    /// bewusst ohne erneutes Persistieren; fehlt der Schlüssel, gilt wieder die
    /// System-Sprache. Löst `languageChanged` aus → main.cpp tauscht den Translator.
    Q_INVOKABLE void reloadLanguage();

    /// Wandelt ein QML-Tasten-Event (Qt::Key + Qt::KeyboardModifiers als int) in einen
    /// kanonischen Akkord-String im Portable-Format um (z. B. "Ctrl+Shift+E"). Reine
    /// Modifier-Tasten liefern "" (für die Hotkey-Aufnahme, QTMUX-15). Nutzt QKeySequence
    /// (QtGui) — hier korrekt, da AppController im Gui-Target lebt; die HotkeyRegistry
    /// bleibt dadurch Gui-frei.
    Q_INVOKABLE QString keyChord(int key, int modifiers) const;

    /// Wandelt einen Action-Shortcut (Q_PROPERTY `shortcut`, ein QVariant) in lesbaren,
    /// plattformkorrekten Anzeigetext für die Menüs um. Akzeptiert sowohl einen
    /// Sequenz-String ("Ctrl+T", aus Hotkeys.bindings) als auch ein StandardKey-Enum
    /// (int, z. B. ZoomIn/Copy). Liefert "" für leere/unbekannte Shortcuts.
    Q_INVOKABLE QString shortcutText(const QVariant &seq) const;

    /// Kopiert Text in die System-Zwischenablage (z. B. ein Vault-Geheimnis).
    Q_INVOKABLE void copyToClipboard(const QString &text) const;

    /// Öffnet ein lokales Verzeichnis (oder eine Datei) im Dateimanager des Systems
    /// (QTMUX-103). Bewusst hier in C++ statt per `"file://" + pfad` in QML: nur
    /// `QUrl::fromLocalFile` kodiert Leerzeichen und Sonderzeichen korrekt und macht
    /// aus `C:\Pfad` unter Windows ein gültiges `file:///C:/Pfad`.
    /// Liefert false, wenn der Pfad leer ist oder das System das Öffnen ablehnt.
    Q_INVOKABLE bool openLocalPath(const QString &path) const;

    /// Die Befehle und Skills, die das Projekt im Arbeitsverzeichnis `workingDir`
    /// mitbringt (QTMUX-96) — für die Befehlspalette. Je Eintrag eine Map mit
    /// `name`, `description`, `source`, `agentId` und `filePath`; ausgewählt wird
    /// schlicht `/<name>` an die Session geschickt.
    ///
    /// `agentId` ist der Agent der **fragenden Session**: ist er gesetzt, kommen nur
    /// dessen eigene und die agentenneutral geteilten Befehle zurück (Regel und
    /// Begründung in `ProjectCommands::filterForAgent`). Leerer Pfad → leere Liste.
    ///
    /// 🔑 Es wird bei JEDEM Aufruf frisch gescannt, bewusst ohne Cache: gemessen
    /// 0 ms für ein Projekt ohne solche Ordner (der Normalfall) und 14,6 ms für
    /// einen Extrembaum mit 72 Skills — beides unter einem Frame, während ein Cache
    /// einen soeben angelegten Befehl verschwiegen hätte.
    Q_INVOKABLE QVariantList projectCommands(const QString &workingDir,
                                             const QString &agentId = QString()) const;

    /// Installierte Monospace-Schriftfamilien (für die Terminal-Schriftwahl).
    Q_INVOKABLE QStringList monospaceFonts() const;
    /// Plattformübliche Standard-Monospace-Familie (Default der Terminal-Schrift).
    Q_INVOKABLE QString defaultMonospaceFont() const;

    /// Startet eine WEITERE, unabhängige QTmux-Instanz als eigener Prozess: eigenes
    /// Instanz-Profil (getrennte QSettings-Domain — eigene Session-Liste/Profile/Vault)
    /// und ein freier MCP-Port, damit sich mehrere Instanzen nicht überlagern. Das ist
    /// dasselbe wie eine Testinstanz (QTMUX_PROFILE/QTMUX_MCP_PORT), nur per Klick.
    /// Liefert den gewählten Port zurück (> 0) oder -1 bei Fehlschlag.
    Q_INVOKABLE int openNewInstance() const;

signals:
    void languageChanged(const QString &lang);
    void reduceMotionChanged();

private:
    QString m_language;
    bool m_reduceMotion = false;
};

} // namespace qtmux

#pragma once

// QTMUX-125: Online-Update — die QTmux-Hälfte über dem vendierten Kern
// (`appupdate`, third_party/updater/). Der Kern kennt weder QSettings noch GUI:
// er holt, prüft die Ed25519-Signatur, lädt herunter und rechnet Entscheidungen
// aus. Persistenz, Zustandsautomat und die Q_PROPERTYs für das QML liegen HIER.
//
// Owner-Entscheidungen (2026-08-02), die den Entwurf festlegen:
//  • Download + geführte Installation — QTmux ersetzt sich NICHT still selbst.
//    `launchInstaller()` startet msiexec/open bzw. tauscht das AppImage; der
//    laufende Prozess bleibt unangetastet.
//  • Stiller Check beim Start, höchstens 1×/Tag, abschaltbar; dazu ein
//    manueller Menüpunkt, der Drosselung UND „Version überspringen" ignoriert.
//  • Downgrade ist erlaubt — aber nur auf ausdrückliche Anforderung und mit
//    Warnung (`isDowngrade`). Der stille Start-Check bietet ihn NIE an, sonst
//    böte eine ältere Veröffentlichung jedem Entwickler-Build täglich an,
//    sich zurückzustufen.
//
// Registrierung: Context-Property `Updates` in main.cpp — KEIN
// qmlRegisterSingletonInstance in die URI „QTmux" (kollidiert mit der
// Modul-Typregistrierung, Symptom „TerminalItem is not a type").

#include <QObject>
#include <QString>
#include <QUrl>

#include <memory>

#include "update/UpdateManifest.hpp"

namespace appupdate {
class UpdateChecker;
}

namespace qtmux {

class UpdateViewModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(int state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY resultChanged)
    Q_PROPERTY(bool isDowngrade READ isDowngrade NOTIFY resultChanged)
    Q_PROPERTY(QString remoteVersion READ remoteVersion NOTIFY resultChanged)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString published READ published NOTIFY resultChanged)
    Q_PROPERTY(QString notes READ notes NOTIFY notesChanged)
    Q_PROPERTY(qint64 downloadSize READ downloadSize NOTIFY resultChanged)
    Q_PROPERTY(double downloadProgress READ downloadProgress NOTIFY downloadProgressChanged)
    Q_PROPERTY(QString downloadedPath READ downloadedPath NOTIFY downloadedPathChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    /// Gibt es für DIESES Betriebssystem überhaupt ein Paket? QTmux liefert alle
    /// drei (DMG/MSI/AppImage) — ein leerer Schlüssel ist trotzdem zulässig und
    /// muss als „kein Paket für dieses System" erscheinen, nicht als Fehler.
    Q_PROPERTY(bool hasPackageForThisSystem READ hasPackageForThisSystem NOTIFY resultChanged)

    /// Einstellung `update/autoCheck` (Vorgabe AN).
    Q_PROPERTY(bool autoCheck READ autoCheck WRITE setAutoCheck NOTIFY autoCheckChanged)
    /// Einstellung `update/baseUrl` — abweichend setzbar für Dry-Runs
    /// (`file:///…`-Fixturebaum oder ein lokaler `python3 -m http.server`).
    Q_PROPERTY(QString baseUrl READ baseUrl WRITE setBaseUrl NOTIFY baseUrlChanged)
    /// Anzeigesprache der Anmerkungen; QML bindet das an `App.language`.
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY notesChanged)

public:
    /// Zustand des Ablaufs. Bewusst ein Enum statt mehrerer Boolesche: der Dialog
    /// zeigt je Zustand genau eine Sache, und widersprüchliche Kombinationen
    /// (busy + fertig) können so gar nicht erst entstehen.
    enum State {
        Idle,          ///< nichts unternommen
        Checking,      ///< Manifest wird geholt/geprüft
        UpToDate,      ///< geprüft, nichts Neueres da
        Available,     ///< neuere Version verfügbar
        Downloading,   ///< Artefakt wird geladen
        Ready,         ///< heruntergeladen und SHA-256-geprüft
        Failed,        ///< Fehler, Text in `lastError`
    };
    Q_ENUM(State)

    explicit UpdateViewModel(QObject *parent = nullptr);
    ~UpdateViewModel() override;

    int state() const { return m_state; }
    bool busy() const { return m_state == Checking || m_state == Downloading; }
    bool updateAvailable() const { return m_available; }
    bool isDowngrade() const { return m_downgrade; }
    QString remoteVersion() const;
    QString currentVersion() const;
    QString published() const;
    QString notes() const;
    qint64 downloadSize() const;
    double downloadProgress() const { return m_progress; }
    QString downloadedPath() const { return m_downloadedPath; }
    QString lastError() const { return m_lastError; }
    bool hasPackageForThisSystem() const;

    bool autoCheck() const;
    void setAutoCheck(bool on);
    QString baseUrl() const;
    void setBaseUrl(const QString &url);
    QString language() const { return m_language; }
    void setLanguage(const QString &lang);

    /// Prüfung von Hand (Menü/Palette): ignoriert die Tagesdrosselung UND eine
    /// übersprungene Version, meldet Fehler sichtbar.
    Q_INVOKABLE void checkNow();

    /// Stiller Start-Check. Tut NICHTS, wenn `autoCheck` aus ist oder heute schon
    /// geprüft wurde; Fehler bleiben still (ein unerreichbarer Server ist kein
    /// Anlass, dem Anwender beim Start einen Dialog zu zeigen).
    Q_INVOKABLE void checkOnStartup();

    /// Lädt das Artefakt dieser Plattform nach `downloadDir()` und prüft den
    /// SHA-256 aus dem Manifest. Bei Abweichung wird die Datei gelöscht.
    Q_INVOKABLE void download();

    /// Startet den heruntergeladenen Installer (msiexec /i · open <dmg> ·
    /// AppImage-Selbstersetzung). Gibt false zurück und füllt `lastError`.
    Q_INVOKABLE bool launchInstaller();

    /// Merkt sich `remoteVersion` als übersprungen — der stille Check schweigt
    /// dazu, bis eine andere Version erscheint. Ein manueller Check zeigt sie weiter.
    Q_INVOKABLE void skipVersion();

    /// Läuft gerade etwas, wird es abgebrochen.
    Q_INVOKABLE void abort();

    /// Zielverzeichnis der Downloads (Downloads/QTmux-Updates, sonst temporär).
    Q_INVOKABLE QString downloadDir() const;

    /// Was `launchInstaller()` TÄTE, als Text — für Dry-Runs und Fehlermeldungen.
    Q_INVOKABLE QString launchPlanDescription() const;

    /// Kann QTmux den Download überhaupt selbst starten?
    /// 🔑 Auf Linux ist die Antwort NEIN, wenn QTmux nicht aus einem AppImage
    /// läuft (Paketverwaltung, Entwickler-Build): Die Selbstersetzung braucht
    /// `$APPIMAGE` als Ziel, und es gibt nichts zu ersetzen. Der Dialog muss das
    /// SAGEN statt einen Knopf anzubieten, der nichts tut — genau so ist der Fall
    /// beim Linux-Build aufgefallen.
    Q_INVOKABLE bool canLaunchInstaller() const;

signals:
    void stateChanged();
    void resultChanged();
    void notesChanged();
    void downloadProgressChanged();
    void downloadedPathChanged();
    void lastErrorChanged();
    void autoCheckChanged();
    void baseUrlChanged();

    /// Es gibt etwas anzubieten — QML öffnet daraufhin den Dialog. Feuert auch
    /// beim stillen Start-Check (das ist sein einziger Zweck).
    void updateFound();
    /// Eine Prüfung ist durch. `manual` unterscheidet den Menüpunkt vom
    /// Start-Check: nur bei `true` darf „Sie sind aktuell" gemeldet werden.
    void checkFinished(bool manual);

private:
    void startCheck(bool manual);
    void setState(State s);
    void setError(const QString &err);
    void rebuildChecker();

    std::unique_ptr<appupdate::UpdateChecker> m_checker;
    std::optional<appupdate::UpdateManifest> m_manifest;
    QString m_checkerBase;   ///< Basis-URL, für die m_checker gebaut wurde

    State m_state = Idle;
    bool m_available = false;
    bool m_downgrade = false;
    bool m_manual = false;
    double m_progress = 0.0;
    QString m_downloadedPath;
    QString m_lastError;
    QString m_language;
};

} // namespace qtmux

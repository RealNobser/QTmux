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
#include "ProxyCredentials.h"

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

    // ---- Proxy-Einstellungen (QTMUX-129) -----------------------------------
    // Die Felder decken sich mit `appupdate::ProxyConfig`. Sie liegen HIER und
    // nicht im QML-`Settings`-Block von Main.qml, weil ihre Schlüssel `update/*`
    // sind — dieselbe Stelle wie `autoCheck`/`baseUrl`.
    // ⚠️ Es gibt bewusst KEIN Passwort: das lebt nur im Sitzungsspeicher
    // (ProxyCredentials.h) und wird nirgends persistiert.
    /// 0 = System (Vorgabe) · 1 = Direkt · 2 = Manuell.
    Q_PROPERTY(int proxyMode READ proxyMode WRITE setProxyMode NOTIFY proxyChanged)
    /// 0 = HTTP · 1 = SOCKS5. Nur bei Modus „Manuell“ von Belang.
    Q_PROPERTY(int proxyType READ proxyType WRITE setProxyType NOTIFY proxyChanged)
    Q_PROPERTY(QString proxyHost READ proxyHost WRITE setProxyHost NOTIFY proxyChanged)
    Q_PROPERTY(int proxyPort READ proxyPort WRITE setProxyPort NOTIFY proxyChanged)
    /// Optional. Wird gespeichert (Komfort), aber NICHT mitexportiert.
    Q_PROPERTY(QString proxyUser READ proxyUser WRITE setProxyUser NOTIFY proxyChanged)

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

    int proxyMode() const;
    void setProxyMode(int mode);
    int proxyType() const;
    void setProxyType(int type);
    QString proxyHost() const;
    void setProxyHost(const QString &host);
    int proxyPort() const;
    void setProxyPort(int port);
    QString proxyUser() const;
    void setProxyUser(const QString &user);

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
    /// Ein Sammelsignal für alle Proxy-Felder — sie werden immer zusammen
    /// gelesen (die Konfiguration ist eine Einheit), einzelne Signale wären nur
    /// fünf Namen mehr ohne Nutzen.
    void proxyChanged();

    /// Es gibt etwas anzubieten — QML öffnet daraufhin den Dialog. Feuert auch
    /// beim stillen Start-Check (das ist sein einziger Zweck).
    void updateFound();
    /// Eine Prüfung ist durch. `manual` unterscheidet den Menüpunkt vom
    /// Start-Check: nur bei `true` darf „Sie sind aktuell" gemeldet werden.
    void checkFinished(bool manual);

    /// Der Proxy verlangt eine Anmeldung, und im Sitzungsspeicher liegt nichts
    /// (mehr). QML öffnet daraufhin `ProxyAuthDialog`. `retry` ist wahr, wenn
    /// zuvor bereits ein Passwort abgelehnt wurde — der Dialog sagt das dann,
    /// statt wortgleich noch einmal zu fragen.
    ///
    /// ⚠️ Wird aus einem **synchronen** Lib-Callback gefeuert und ist deshalb
    /// eine reine Benachrichtigung: Die laufende Anfrage ist damit beendet
    /// (`ErrorKind::ProxyAuthenticationRequired`). Die Antwort kommt später über
    /// `provideProxyCredentials()` und startet den Vorgang **neu**.
    void proxyAuthenticationNeeded(const QString &host, int port, bool retry);

public:
    // ---- Proxy-Anmeldung: der Weg von QML zurück in die Lib (QTMUX-129) -----
    //
    // 🔑 Warum zweistufig und nicht „Dialog beantwortet das Signal": Qts
    // `proxyAuthenticationRequired` wird **synchron** zugestellt, der
    // `QAuthenticator*` gilt nur während des Slot-Aufrufs. Ein QML-Dialog
    // antwortet asynchron — der Slot wäre längst zurück. Bliebe eine
    // verschachtelte Event-Loop mitten im Netzwerk-Callback, und genau diese
    // Klasse Fehler hat uns der `busy()`-Fall schon einmal gekostet.
    // Deshalb: Die Lib fragt einen **Lieferanten**, der nur aus dem Speicher
    // antwortet und nie blockiert; fehlt etwas, scheitert die Anfrage sauber
    // und QML füllt nach.

    /// Aus dem Dialog. Legt die Anmeldedaten in den **Sitzungsspeicher** (nur
    /// RAM, s. ProxyCredentials.h) und wiederholt den unterbrochenen Vorgang.
    Q_INVOKABLE void provideProxyCredentials(const QString &user, const QString &password);

    /// Der Mensch bricht ab: Speicher leeren, nicht erneut fragen.
    Q_INVOKABLE void cancelProxyAuthentication();

    /// Anmeldedaten dieser Sitzung vergessen (Proxy abgeschaltet, Einstellungen
    /// geändert, Programmende).
    Q_INVOKABLE void forgetProxyCredentials();

    /// Liegt für diese Sitzung ein Passwort im Speicher?
    Q_INVOKABLE bool hasProxyCredentials() const;

    /// **Der Anschlusspunkt für den Lib-Lieferanten.** Bewusst mit eigenen Typen
    /// statt mit `appupdate`-Typen: So ist die App-Hälfte fertig und testbar,
    /// bevor der Kern nachvendiert ist — angeschlossen wird sie später mit einer
    /// einzigen Lambda-Zeile an `setProxyCredentialProvider`.
    ///
    /// Gibt `true` zurück, wenn `user`/`password` gefüllt wurden. Bei `false`
    /// soll der Aufrufer aufgeben (**nicht** wiederholen — Kontosperre).
    bool answerProxyChallenge(const QString &host, int port, bool previousAttemptFailed,
                              QString *user, QString *password);

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

    /// Nur im Arbeitsspeicher, nie persistiert — s. ProxyCredentials.h.
    ProxyCredentials m_proxyCreds;
    /// Was zuletzt lief, damit `provideProxyCredentials()` es wiederholen kann.
    /// Ohne das müsste der Mensch nach der Anmeldung von Hand neu anstoßen.
    bool m_proxyRetryIsDownload = false;
    bool m_proxyRetryManual = false;
};

} // namespace qtmux

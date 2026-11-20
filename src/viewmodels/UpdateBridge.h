#pragma once

// Online-Update, Vertrag V2 (K2): die QTmux-Verdrahtung des gemeinsamen Ablaufs.
//
// Der Ablauf selbst ist `appupdate::UpdateFlow` aus dem vendierten Hub-Kern
// (third_party/updater/update/). Er entscheidet ALLES: Zustände, Texte, Knöpfe,
// Hinweis, Fristen, Einstellungsschlüssel und deren Übernahme aus den alten
// QTmux-Schreibweisen. Diese Klasse entscheidet NICHTS. Sie
//   • gibt dem Ablauf, was der Vertrag (§4.6) von der App verlangt: Produkt,
//     Version und Build-ID, Sprache, den Beenden-Rückruf und „was endet“;
//   • macht dem QML zugänglich, was der Ablauf als C++-API anbietet: die
//     Aktionen als Zahl (QML kann den Enum-Typ nicht übergeben) und die
//     Einstellungen als Properties mit Änderungssignal;
//   • führt aus, was Gui-Code braucht und darum nicht im Kern stehen darf
//     („im Dateimanager zeigen“).
// Ein Wächter (tests/CheckUpdateConsumer.cmake) belegt, dass im App-Code kein
// Aufruf von UpdateChecker, kein eigener `update/`-Schlüssel und kein
// Versionsvergleich steht.
//
// Registrierung: Context-Property `Updates` in main.cpp (KEIN
// qmlRegisterSingletonInstance in die URI „QTmux“, s. Konventionen).

#include <QObject>
#include <QSettings>
#include <QString>
#include <QVariantMap>

#include <functional>
#include <memory>

#include "update/UpdateFlow.hpp"

namespace qtmux {

class UpdateBridge : public QObject {
    Q_OBJECT
    /// Der Ablauf selbst — QML bindet an `Updates.flow.view`, `.hint`, `.notes`,
    /// `.proxyAuth`, `.state`, `.busy`.
    Q_PROPERTY(QObject *flow READ flowObject CONSTANT)
    /// Reine Versionsnummer (Vergleich macht der Ablauf) und volle Build-ID (Anzeige).
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString buildId READ buildId CONSTANT)

    // --- Einstellungsseite „Updates“ (Vertrag §5; Familienregel „wirkt sofort“).
    // Jeder Setter schreibt sofort über den Ablauf; gelesen wird immer frisch aus
    // den Einstellungen, damit ein Import oder Zurücksetzen sofort sichtbar ist.
    Q_PROPERTY(bool autoCheck READ autoCheck WRITE setAutoCheck NOTIFY settingsChanged)
    /// 0 = System, 1 = Direkt, 2 = Manuell — dieselbe Reihenfolge wie
    /// `appupdate::ProxyConfig::Mode` und wie die Auswahl auf der Seite.
    Q_PROPERTY(int proxyMode READ proxyMode WRITE setProxyMode NOTIFY settingsChanged)
    /// 0 = HTTP, 1 = SOCKS5.
    Q_PROPERTY(int proxyType READ proxyType WRITE setProxyType NOTIFY settingsChanged)
    Q_PROPERTY(QString proxyHost READ proxyHost WRITE setProxyHost NOTIFY settingsChanged)
    /// 0 = nicht gesetzt.
    Q_PROPERTY(int proxyPort READ proxyPort WRITE setProxyPort NOTIFY settingsChanged)
    Q_PROPERTY(QString proxyUser READ proxyUser WRITE setProxyUser NOTIFY settingsChanged)
    /// Leer = keine Version übersprungen (sichtbar und zurücksetzbar, A25).
    Q_PROPERTY(QString skippedVersion READ skippedVersion NOTIFY settingsChanged)
    Q_PROPERTY(bool hasSessionPassword READ hasSessionPassword NOTIFY sessionPasswordChanged)

    /// Zahl der Sitzungen, die beim Beenden enden. QML bindet sie an das
    /// SessionModel (Main.qml); der Ablauf liest sie über `quitImpact()` und macht
    /// daraus den Satz im Zustand „Bereit“.
    Q_PROPERTY(int runningSessions READ runningSessions WRITE setRunningSessions NOTIFY
                   runningSessionsChanged)

    /// „Das Wichtigste“ der Änderungshinweise (erster Absatz, HTML-Teilset des Hubs);
    /// `flow.notes` trägt nur die Gruppen und Punkte.
    Q_PROPERTY(QString notesHighlight READ notesHighlight NOTIFY notesChanged)
    /// Zählt bei jedem Sprachwechsel hoch. QML liest ihn in Bindungen an
    /// `text()` und `flow.view` mit, damit sie neu ausgewertet werden.
    Q_PROPERTY(int textRevision READ textRevision NOTIFY textsChanged)

public:
    struct Wiring {
        /// UI-Sprache beim Start ("de" | "en").
        QString language;
        /// Beendet QTmux über den normalen Weg (src/app/UpdateQuit.cpp). Liefert
        /// false, wenn das Schließen abgelehnt wurde.
        std::function<bool()> requestQuit;
        /// Nur im Selbsttest (src/app/main.cpp, Schalter `--selftest-update`):
        /// erlaubt dem Ablauf `--update-base-url`/`--update-channel`. Dann ist JEDE
        /// Installation ein Trockenlauf (Hub-Regel).
        bool selfTest = false;
        /// Nur im Selbsttest: Ordner für Download und Ergebnisdatei, damit kein
        /// Testlauf den geschützten Ordner „Downloads“ berührt (TCC).
        QString selfTestDir;
    };

    UpdateBridge(Wiring wiring, QObject *parent = nullptr);
    ~UpdateBridge() override;

    [[nodiscard]] appupdate::UpdateFlow &flow() { return *m_flow; }
    [[nodiscard]] QObject *flowObject() { return m_flow.get(); }
    [[nodiscard]] QString currentVersion() const;
    [[nodiscard]] QString buildId() const;
    /// UpgradeCode des MSI (installer/QTmux.wxs); ein Test hält beide gleich.
    [[nodiscard]] static QString windowsUpgradeCode();

    [[nodiscard]] bool autoCheck() const;
    void setAutoCheck(bool on);
    [[nodiscard]] int proxyMode() const;
    void setProxyMode(int mode);
    [[nodiscard]] int proxyType() const;
    void setProxyType(int type);
    [[nodiscard]] QString proxyHost() const;
    void setProxyHost(const QString &host);
    [[nodiscard]] int proxyPort() const;
    void setProxyPort(int port);
    [[nodiscard]] QString proxyUser() const;
    void setProxyUser(const QString &user);
    [[nodiscard]] QString skippedVersion() const;
    [[nodiscard]] bool hasSessionPassword() const;

    [[nodiscard]] int runningSessions() const { return m_runningSessions; }
    void setRunningSessions(int n);
    [[nodiscard]] QString notesHighlight() const;
    [[nodiscard]] int textRevision() const { return m_textRevision; }

    /// Satz des Hub-Katalogs (Vertrag V2 §4.5) — QTmux führt keinen eigenen
    /// Wortlaut für den Update-Weg. `args` füllt die Platzhalter ({neu}, {n} …).
    Q_INVOKABLE QString text(const QString &key, const QVariantMap &args = {}) const;
    /// qrc-Pfad eines Icons, das der Ablauf nennt; leer (mit Warnung) für einen
    /// Namen außerhalb von `UpdateFlow::iconNames()` und den Icons der Oberfläche.
    Q_INVOKABLE QString iconSource(const QString &name) const;

    /// Knopf der Oberfläche: `action` ist `appupdate::UpdateFlow::Action` als Zahl,
    /// so wie sie in `flow.view.buttons[i].action` steht.
    Q_INVOKABLE bool perform(int action);
    /// Menü „Nach Updates suchen …“ (Ablauf entscheidet: zeigen oder prüfen).
    Q_INVOKABLE bool openByHand();
    Q_INVOKABLE bool openFromHint();
    Q_INVOKABLE void dismissHint();
    Q_INVOKABLE void setDialogVisible(bool visible);
    Q_INVOKABLE void provideProxyCredentials(const QString &user, const QString &password,
                                             bool remember);
    Q_INVOKABLE void cancelProxyAuth();
    /// Einstellungsseite: „Jetzt prüfen“.
    Q_INVOKABLE bool checkNow();
    Q_INVOKABLE void resetSkippedVersion();
    Q_INVOKABLE void forgetSessionPassword();
    /// „Diese Seite zurücksetzen“ der Seite „Updates“ (Vorgaben der Familie).
    Q_INVOKABLE void restoreDefaults();
    /// Nach Import/Zurücksetzen von außen (SettingsIo): Properties neu lesen.
    Q_INVOKABLE void reloadSettings();

    /// Sprachwechsel zur Laufzeit (QTmux: „wirkt sofort“).
    void setLanguage(const QString &language);

    /// Nach dem ersten Aufbau des Hauptfensters: Übernahme alter Schlüssel,
    /// Auswertung einer begonnenen Installation, stille Prüfungen.
    void start(const QStringList &arguments);

signals:
    void settingsChanged();
    void runningSessionsChanged();
    void sessionPasswordChanged();
    /// Nur Bildmodus: Main.qml öffnet Einstellungen › Updates.
    void settingsPageRequested();
    void notesChanged();
    void textsChanged();

private:
    void setProxy(const std::function<void(appupdate::ProxyConfig &)> &change);

    QSettings m_settings;
    std::unique_ptr<appupdate::UpdateFlow> m_flow;
    int m_runningSessions = 0;
    int m_textRevision = 0;
    bool m_selfTest = false;
};

} // namespace qtmux

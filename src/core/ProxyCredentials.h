#pragma once

// qtmux::ProxyCredentials — Sitzungsspeicher für Proxy-Anmeldedaten (QTMUX-129,
// App-Hälfte der Proxy-Unterstützung).
//
// 🔑 NUR IM ARBEITSSPEICHER, für die Laufzeit dieses Programms. Nichts hiervon
// geht je nach QSettings, in eine Exportdatei oder in den Vault. Im Firmenumfeld
// sind das typischerweise **Domänen**-Anmeldedaten; eine Klartext-INI verteilt sie
// über Time Machine, OneDrive und servergespeicherte Profile.
//
// Warum nicht der SecretsVault (er verwaltet ja schon SSH-Passwörter): Er ist
// master-passwortgeschützt und startet **gesperrt**. Der stille Start-Check auf
// Updates müsste also nach dem Master-Passwort fragen — genau das verbietet die
// Owner-Regel „Fehler und Rückfragen beim Start bleiben still". Ein Keychain
// wiederum wären drei Implementierungen (Security.framework · CredMan · libsecret),
// und der Linux-Teil endete wie beim SleepInhibitor als Stub — ausgerechnet dort,
// wo Firmen-Linux steht.
//
// ⚠️ DIE WICHTIGE REGEL STEHT HIER, NICHT IN DER LIB. `appupdate` fragt über einen
// Anmelde-Lieferanten und reicht dabei `previousAttemptFailed` durch — sie
// **erzwingt aber keine Obergrenze**, die App entscheidet, wann sie aufgibt
// (gelesen an MacPCAN `0934eff`). Wiederholtes Anmelden mit einem falschen
// Passwort sperrt in einer AD-Umgebung nach wenigen Versuchen das Konto. Deshalb
// gilt hier: **ein gesetztes Passwort wird genau EINMAL angeboten.** Kommt die
// Frage ein zweites Mal, war es falsch → verwerfen und den Menschen fragen.
// Dieselbe Linie wie beim SSH-Passwort-Auto-Fill („genau einmal senden — kein
// Lockout").

#include <QString>

namespace qtmux {

class ProxyCredentials {
public:
    /// Aus dem Dialog übernommen. Setzt die Ein-Versuch-Sperre zurück: neue
    /// Anmeldedaten verdienen einen eigenen Versuch.
    void set(const QString &user, const QString &password);

    /// Alles vergessen. Aufzurufen bei „Proxy aus", beim Wechsel der
    /// Proxy-Einstellungen und am Programmende.
    void clear();

    [[nodiscard]] QString user() const { return m_user; }
    [[nodiscard]] QString password() const { return m_password; }

    /// Liegt ein vollständiges, noch nicht verbrauchtes Paar vor?
    [[nodiscard]] bool hasUsablePassword() const
    {
        return !m_password.isEmpty() && !m_offered;
    }

    /// **Die Regel.** Antwort auf „darf ich der Lib jetzt Anmeldedaten geben?".
    ///
    /// `previousAttemptFailed` kommt von `appupdate` und ist ab der zweiten
    /// Aufforderung derselben Anfrage wahr. Dann wird hier **immer** abgelehnt
    /// und das Passwort verworfen — der Aufrufer beendet die Anfrage damit
    /// sauber, statt in eine Kontosperre zu laufen.
    ///
    /// Der Benutzername bleibt bewusst stehen: Er ist kein Geheimnis, und beim
    /// erneuten Fragen muss der Mensch nicht `DOMÄNE\name` noch einmal tippen.
    [[nodiscard]] bool mayAnswer(bool previousAttemptFailed);

    /// Wurde das zuletzt gesetzte Passwort von der Gegenstelle abgelehnt? Treibt
    /// die Zeile „Anmeldung fehlgeschlagen" im Dialog — ohne sie fragt er beim
    /// zweiten Mal wortgleich noch einmal und wirkt kaputt.
    [[nodiscard]] bool lastAttemptRejected() const { return m_rejected; }

private:
    QString m_user;
    QString m_password;
    /// Schon einmal herausgegeben? Dann ist der eine Versuch verbraucht.
    bool m_offered = false;
    bool m_rejected = false;
};

}  // namespace qtmux

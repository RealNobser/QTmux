#pragma once

#include <QByteArray>
#include <QHostAddress>
#include <QString>

/// Zugriffsregeln des eingebetteten MCP-Servers (QTMUX-127): an welche Adresse er
/// bindet, ob dabei ein Token Pflicht ist und ob er unter den gegebenen Umständen
/// überhaupt starten darf.
///
/// 🔑 Bewusst eine EIGENE, Gui-freie Einheit neben `McpServer` — wie `RestoreMode.h`
/// oder `TerminalGrid.h`: Die Regel „nicht-lokal binden nur mit Token" ist eine
/// Sicherheitsentscheidung und muss an genau EINER Stelle stehen und testbar sein
/// (`tests/tst_mcpaccess.cpp`). Sie im Server zu verstreuen hieße, sie beim nächsten
/// Umbau versehentlich zu lockern.
namespace qtmux::mcpaccess {

/// Ergebnis der Adress-Auflösung. `address` ist IMMER benutzbar: Ist die Eingabe
/// ungültig, steht hier Loopback und `error` nennt den Grund — ein stilles
/// `QHostAddress::Any` wäre die gefährlichste aller Fehlinterpretationen.
struct Bind {
    QHostAddress address{QHostAddress::LocalHost};
    QString      requested;             ///< Rohtext, wie er konfiguriert war
    QString      error;                 ///< leer = Eingabe war gültig
    bool         fromEnvironment = false;  ///< Quelle war QTMUX_MCP_BIND

    bool isLoopback() const { return address.isLoopback(); }
    /// Textform der EFFEKTIVEN Adresse (nicht der angefragten) — für Anzeige und Log.
    QString text() const { return address.toString(); }
};

/// Wandelt einen konfigurierten Adresstext in eine Bind-Adresse. Erlaubt sind
/// IP-Literale sowie die Schlüsselwörter `localhost`, `any` und `*`; leer = Loopback.
/// **Kein DNS** — eine Namensauflösung beim Start würde blockieren, und ein Name,
/// der auf eine fremde Adresse zeigt, wäre eine Sicherheitsüberraschung.
Bind resolveBind(const QString &raw);

/// Konfigurierter Adresstext: `QTMUX_MCP_BIND` > Einstellung `mcp/bindAddress` > leer.
/// `fromEnv` meldet, ob die Umgebung die Quelle war (entscheidet über die
/// Auto-Erzeugung des Tokens, s. `shouldAutoGenerateToken`).
QString configuredBindText(bool *fromEnv = nullptr);

/// `configuredBindText()` aufgelöst — die Vorgabe des Servers beim Start.
Bind defaultBind();

/// Token: `QTMUX_MCP_TOKEN` > Einstellung `mcp/token`; leer = keins konfiguriert.
QString resolveToken();
/// Meldet, ob das Token aus der Umgebung stammt (dann ist es in der GUI nicht änderbar).
bool tokenFromEnvironment();

/// 32 kryptografisch zufällige Bytes, base64url ohne Polsterung (43 Zeichen).
QString generateToken();

/// Zeitkonstanter Vergleich — **kein** `==` auf QByteArray: dessen Abbruch beim ersten
/// abweichenden Byte verrät über die Antwortzeit, wie viele Zeichen stimmten.
/// Ein leeres `expected` wird nie akzeptiert (sonst öffnete eine fehlende
/// Konfiguration den Server für jeden).
bool tokenAccepted(const QByteArray &presented, const QByteArray &expected);

/// Zieht `<token>` aus einem `Authorization: Bearer <token>`-Header. `headerBlock` ist
/// der komplette HTTP-Kopf (ohne Leerzeile); leer, wenn kein Bearer-Header dasteht.
QByteArray bearerToken(const QByteArray &headerBlock);

/// Braucht diese Bindung eine Token-Prüfung? Loopback nein (dort ist die
/// Prozessgrenze die Kontrolle, und bestehende lokale Clients sollen unverändert
/// laufen), alles andere ja.
inline bool authRequiredFor(const Bind &b) { return !b.isLoopback(); }

/// Darf beim Start ein Token erzeugt und gespeichert werden, wenn keins existiert?
/// Nur wenn die Öffnung aus der **Einstellung** kommt: Dann gibt es eine Oberfläche,
/// die das erzeugte Token anzeigen kann. Kommt sie aus `QTMUX_MCP_BIND` (Skript, CI,
/// Kiosk), wäre ein selbst erzeugtes Token eines, das niemand zu sehen bekommt — dort
/// ist die Startverweigerung die ehrlichere Antwort.
inline bool shouldAutoGenerateToken(const Bind &b, const QString &token) {
    return authRequiredFor(b) && token.isEmpty() && !b.fromEnvironment;
}

/// Startfreigabe.
struct StartCheck {
    bool    allowed = true;
    bool    authRequired = false;
    QString reason;   ///< nur gesetzt, wenn `allowed` false ist
};
StartCheck checkStart(const Bind &bind, const QString &token);

} // namespace qtmux::mcpaccess

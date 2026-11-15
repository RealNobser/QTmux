#pragma once

#include <QByteArray>
#include <QString>

namespace qtmux {

/// **Nachrichten-Eingang eines Agenten** — der Weg, einem Agenten etwas zuzustellen, OHNE
/// in seine Eingabezeile zu schreiben. Gui-frei und ohne Netz: Hier liegen nur die Regeln
/// (welche Gegenstelle ist plausibel, wie sieht der Rahmen aus, wie heißt der Absender);
/// das Schreiben auf den Socket macht `AgentInboxClient` in `src/server`.
///
/// 🔑 **Der Agent meldet seine Gegenstelle selbst** (MCP `set_agent_inbox`, typisch aus
/// einem Start-Hook). QTmux sucht sie nicht in den Dateien des Agenten — dieselbe Linie wie
/// bei der Fortsetzungs-Kennung (QTMUX-98): Was ein fremdes Programm intern ablegt, ist
/// dessen Sache und kann sich mit jedem Update ändern.
///
/// Welche Arten es gibt, steht in `inboxKindKnown`. Derzeit genau eine: `claude-code` —
/// der Unix-Socket, den Claude Code je Sitzung für „Nachrichten zwischen Sitzungen“ öffnet
/// und seinen Hooks als `CLAUDE_CODE_MESSAGING_SOCKET` mitgibt.
namespace agentinbox {

inline constexpr char kKindClaudeCode[] = "claude-code";

/// Obergrenze für den Text einer Nachricht (Bytes, UTF-8). Claude Code nimmt deutlich
/// mehr; die Grenze hält den Schreibvorgang kurz, weil er im GUI-Thread läuft.
inline constexpr int kMaxTextBytes = 200000;

/// Fassung von Claude Code, an der der Rahmen zuletzt am lebenden Objekt belegt wurde.
/// ⚠️ Der Rahmen ist NICHT dokumentiert (nur die Umgebungsvariable und die
/// Anmeldezeile sind es). Meldet ein Agent eine andere Fassung, sagt die Antwort von
/// `send_message` das dazu — die Zustellung selbst kann QTmux nicht prüfen, der Socket
/// antwortet nicht.
inline constexpr char kClaudeCodeVerifiedVersion[] = "2.1.323";

bool inboxKindKnown(const QString &kind);

/// Formale Prüfung einer gemeldeten Gegenstelle, ohne das Dateisystem anzufassen. Leer =
/// in Ordnung, sonst ein Satz, der sagt, was nicht stimmt. Verlangt wird ein absoluter
/// Pfad auf eine `.sock`-Datei ohne Steuerzeichen und ohne `..`, höchstens 103 Bytes lang
/// (länger kann ein Unix-Socket nicht heißen).
QString endpointProblem(const QString &kind, const QString &address);

/// Anzeigename des Absenders für den Rahmen — bereits so normalisiert, wie Claude Code
/// ihn selbst normalisieren würde (sonst verwirft der Empfänger die Namensangabe):
/// höchstens 60 Zeichen, nur Buchstaben, Ziffern, Leerzeichen und `- _ . , : ( ) # / +`.
/// Leer, wenn nichts Verwertbares übrig bleibt.
QString frameSenderName(const QString &raw);

/// **Der Rahmen für `claude-code`**: eine JSON-Zeile samt abschließendem `\n`, so wie
/// Claude Code sie zwischen zwei Sitzungen schickt (am 2026-11-15 an 2.1.323
/// mitgeschnitten). `msgId` ist eine UUID. Der Rahmen trägt bewusst KEINE
/// Absender-Adresse und keine Angabe zum Rechte-Modus: QTmux ist keine Claude-Sitzung und
/// behauptet nicht, eine zu sein — der Empfänger führt die Nachricht dann als
/// „unidentified session“, der Name erscheint als Angabe des Absenders.
/// Ein im Text enthaltenes Ende-Tag des Rahmens wird entschärft (es schlösse ihn von
/// innen).
QByteArray claudeCodeFrame(const QString &text, const QString &senderName, const QString &msgId);

/// Vergleicht zwei Fassungen der Form `a.b.c`. < 0: `a` älter, 0: gleich bzw. nicht
/// vergleichbar, > 0: `a` neuer.
int compareVersions(const QString &a, const QString &b);

} // namespace agentinbox
} // namespace qtmux

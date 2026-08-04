<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="en_US">
<context>
    <name>CatAgenten</name>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="16"/>
        <source>Agenten &amp; MCP</source>
        <translation>Agents &amp; MCP</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="17"/>
        <source>Steuerung durch KI-Agenten und Benachrichtigungen zwischen Sitzungen.</source>
        <translation>Control by AI agents and notifications between sessions.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="81"/>
        <source>fertig</source>
        <translation>done</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="82"/>
        <source>Frage</source>
        <translation>question</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="83"/>
        <source>Fehler</source>
        <translation>error</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="94"/>
        <source>Wiederherstellung</source>
        <translation>Restore</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="96"/>
        <source>Agenten beim Start wiederherstellen</source>
        <translation>Restore agents on start</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="97"/>
        <source>Setzt in jedem Pane den zuletzt erkannten Agenten erneut ab, sobald die Shell bereit ist. Es wird ausschließlich ein bekannter Agent gestartet — beliebige Befehle laufen nicht automatisch los.</source>
        <translation>Re-runs the last detected agent in every pane as soon as the shell is ready. Only a known agent is started — arbitrary commands are never run automatically.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="109"/>
        <source>Unterhaltung fortsetzen</source>
        <translation>Continue conversation</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="315"/>
        <source>MCP-Server aktiv</source>
        <translation>MCP server active</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="318"/>
        <source>Erreichbar unter %1:%2. Der Secrets-Vault ist über MCP bewusst NICHT erreichbar, und die Einstellungen dieser Gruppe lassen sich über MCP nicht ändern.</source>
        <translation>Reachable at %1:%2. The secrets vault is deliberately NOT exposed via MCP, and the settings in this group cannot be changed via MCP.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="355"/>
        <source>Im Netzwerk erreichbar</source>
        <translation>Reachable over the network</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="357"/>
        <source>Vorgegeben durch QTMUX_MCP_BIND — diese Einstellung wirkt gerade nicht.</source>
        <translation>Set by QTMUX_MCP_BIND — this setting has no effect right now.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="360"/>
        <source>Andere Rechner können QTmux fernsteuern. Über MCP lässt sich beliebiger Text in laufende Terminals schreiben, deshalb ist das Token Pflicht. Zusätzlich auf Netzebene einschränken (macOS: pf, s. tools/pf/).</source>
        <translation>Other machines can remote-control QTmux. MCP can type arbitrary text into running terminals, which is why the token is mandatory. Restrict it at the network level as well (macOS: pf, see tools/pf/).</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="364"/>
        <source>Aus: nur Programme auf diesem Rechner (127.0.0.1) — das ist die Vorgabe und braucht kein Token.</source>
        <translation>Off: only programs on this machine (127.0.0.1) — this is the default and needs no token.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="373"/>
        <source>Bind-Adresse</source>
        <translation>Bind address</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="376"/>
        <source>127.0.0.1 = nur dieser Rechner · 0.0.0.0 = alle Schnittstellen · oder eine bestimmte Adresse wie 192.168.0.10. QTMUX_MCP_BIND hat Vorrang.</source>
        <translation>127.0.0.1 = this machine only · 0.0.0.0 = all interfaces · or a specific address such as 192.168.0.10. QTMUX_MCP_BIND takes precedence.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="395"/>
        <source>Zugriffs-Token</source>
        <translation>Access token</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="397"/>
        <source>Kommt aus QTMUX_MCP_TOKEN — hier nicht änderbar.</source>
        <translation>Comes from QTMUX_MCP_TOKEN — cannot be changed here.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="399"/>
        <source>Clients schicken es als Kopfzeile „Authorization: Bearer &lt;token&gt;“; ohne gültiges Token antwortet der Server mit 401.</source>
        <translation>Clients send it as an “Authorization: Bearer &lt;token&gt;” header; without a valid token the server replies 401.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="402"/>
        <source>Wird erst geprüft, wenn der Server im Netzwerk erreichbar ist. Lokale Clients brauchen keins.</source>
        <translation>Only checked once the server is reachable over the network. Local clients need none.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="412"/>
        <source>kein Token</source>
        <translation>no token</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="416"/>
        <source>Verbergen</source>
        <translation>Hide</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="416"/>
        <source>Anzeigen</source>
        <translation>Show</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="422"/>
        <source>Kopieren</source>
        <translation>Copy</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="428"/>
        <source>Neu erzeugen</source>
        <translation>Generate new</translation>
    </message>
    <message>
        <source>Hört ausschließlich auf 127.0.0.1; der Secrets-Vault ist über MCP bewusst NICHT erreichbar.</source>
        <translation type="vanished">Listens on 127.0.0.1 only; the secrets vault is deliberately NOT reachable via MCP.</translation>
    </message>
    <message>
        <source>Hängt das Fortsetzungs-Argument des Agenten an (z. B. --continue), sodass er die vorherige Unterhaltung weiterführt. Nur bei Agenten, die das können; ohne vorherige Unterhaltung meldet der Agent einen Fehler.</source>
        <translation type="vanished">Appends the agent&apos;s continuation argument (e.g. --continue) so it picks up the previous conversation. Only for agents that support it; without a previous conversation the agent reports an error.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="130"/>
        <source>Gar nicht</source>
        <translation>Not at all</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="130"/>
        <source>Jüngste im Verzeichnis</source>
        <translation>Most recent in directory</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="131"/>
        <source>Auswahl beim Start</source>
        <translation>Pick at startup</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="131"/>
        <source>Gemeldete Sitzung</source>
        <translation>Reported session</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="112"/>
        <source>Der Agent nimmt die JÜNGSTE Unterhaltung seines Arbeitsverzeichnisses. Richtig, solange dort nur ein Agent arbeitet — laufen mehrere im selben Ordner, bekommen sie alle dieselbe.</source>
        <translation>The agent picks the MOST RECENT conversation in its working directory. Correct as long as only one agent works there — if several run in the same folder, they all get the same one.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="115"/>
        <source>Der Agent öffnet beim Start seine eigene Auswahlliste; du entscheidest je Pane. Es wird nichts geraten, kostet aber einen Klick. Derzeit bietet nur Claude Code eine solche Liste an.</source>
        <translation>The agent opens its own picker at startup; you decide per pane. Nothing is guessed, but it costs a click. Currently only Claude Code offers such a list.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="118"/>
        <source>Genau die Unterhaltung, die der Agent zuletzt selbst gemeldet hat (MCP-Werkzeug set_agent_session) — auch bei mehreren Agenten im selben Ordner eindeutig. Meldet er nichts, startet er frisch. QTmux kann die Kennung nicht selbst ermitteln: sie entsteht im Agenten und ändert sich bei /resume oder /clear.</source>
        <translation>Exactly the conversation the agent last reported itself (MCP tool set_agent_session) — unambiguous even with several agents in the same folder. If it reports nothing, it starts fresh. QTmux cannot determine the identifier itself: it originates inside the agent and changes on /resume or /clear.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="123"/>
        <source>Der Agent startet mit einer frischen Unterhaltung.</source>
        <translation>The agent starts with a fresh conversation.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="144"/>
        <source>Benachrichtigungen</source>
        <translation>Notifications</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="146"/>
        <source>Zeile = Empfänger, Spalte = Quelle. Ein Häkchen bedeutet: der Empfänger wird über Ereignisse der Quelle benachrichtigt. Agenten abonnieren sich meist selbst per MCP (subscribe_events).</source>
        <translation>Row = receiver, column = source. A check means the receiver is notified about the source&apos;s events. Agents usually subscribe themselves via MCP (subscribe_events).</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="196"/>
        <source>Arten</source>
        <translation>Types</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="303"/>
        <source>Keine Sessions geöffnet.</source>
        <translation>No sessions open.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="313"/>
        <source>Agenten-Steuerung (MCP)</source>
        <translation>Agent control (MCP)</translation>
    </message>
    <message>
        <source>MCP-Server aktiv (nur 127.0.0.1)</source>
        <translation type="vanished">MCP server enabled (127.0.0.1 only)</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="328"/>
        <source>Port</source>
        <translation>Port</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="333"/>
        <source>Wird gespeichert und beim nächsten Start verwendet; die Umgebungsvariable QTMUX_MCP_PORT hat Vorrang.</source>
        <translation>Stored and used on the next start; the QTMUX_MCP_PORT environment variable takes precedence.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="344"/>
        <location filename="../qml/prefs/CatAgenten.qml" line="387"/>
        <source>Übernehmen</source>
        <translation>Apply</translation>
    </message>
    <message>
        <source>Nur 127.0.0.1 · Vault nicht über MCP erreichbar. Wird gespeichert und beim nächsten Start verwendet; QTMUX_MCP_PORT hat Vorrang.</source>
        <translation type="vanished">127.0.0.1 only · Vault not reachable via MCP. Saved and used on next start; QTMUX_MCP_PORT takes precedence.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="444"/>
        <source>Bitte einen Port zwischen 1024 und 65535 angeben.</source>
        <translation>Please enter a port between 1024 and 65535.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="450"/>
        <source>Port %1 ließ sich nicht öffnen (belegt?). Server ist aus.</source>
        <translation>Port %1 could not be opened (already in use?). Server is off.</translation>
    </message>
</context>
<context>
    <name>CatAllgemein</name>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="12"/>
        <source>Allgemein</source>
        <translation>General</translation>
    </message>
    <message>
        <source>Sprache, Erscheinungs-Modus und Verhalten beim Beenden.</source>
        <translation type="vanished">Language, appearance mode and quit behavior.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="21"/>
        <source>Design</source>
        <translation>Theme</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="24"/>
        <source>Wie System</source>
        <translation>Follow System</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="24"/>
        <source>Hell</source>
        <translation>Light</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="24"/>
        <source>Dunkel</source>
        <translation>Dark</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="30"/>
        <source>Sprache</source>
        <translation>Language</translation>
    </message>
    <message>
        <source>Fenster</source>
        <translation type="vanished">Window</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="87"/>
        <source>Vor dem Beenden nachfragen</source>
        <translation>Ask before quitting</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="88"/>
        <source>Beenden schließt alle Sitzungen samt laufender Prozesse.</source>
        <translation>Quitting closes all sessions along with their running processes.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="54"/>
        <source>Sessions beim Start wiederherstellen</source>
        <translation>Restore sessions on start</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="13"/>
        <source>Sprache, Erscheinungs-Modus und Verhalten beim Start und Beenden.</source>
        <translation>Language, appearance mode and behaviour on start and quit.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="19"/>
        <source>Sprache &amp; Design</source>
        <translation>Language &amp; appearance</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="22"/>
        <source>„Wie System“ folgt der Einstellung des Betriebssystems.</source>
        <translation>“Follow system” follows the operating system setting.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="31"/>
        <source>Wirkt sofort; das native macOS-App-Menü folgt erst nach einem Neustart.</source>
        <translation>Takes effect immediately; the native macOS app menu follows after a restart.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="49"/>
        <source>Start &amp; Beenden</source>
        <translation>Start &amp; quit</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="57"/>
        <source>QTmux startet mit einer einzelnen, leeren Session. Der zuletzt gespeicherte Stand bleibt erhalten — er wird beim Beenden nicht überschrieben und ist wieder da, sobald hier erneut wiederhergestellt wird.</source>
        <translation>QTmux starts with a single, empty session. The last saved state is kept — it is not overwritten on quit and returns as soon as restoring is enabled here again.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="69"/>
        <source>Gar nicht</source>
        <translation>Not at all</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="69"/>
        <source>Ohne Verlauf</source>
        <translation>Without history</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="69"/>
        <source>Alles</source>
        <translation>Everything</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="75"/>
        <source>Agenten in den Panes</source>
        <translation>Agents in the panes</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="76"/>
        <source>Ob die Agenten dabei erneut starten und ihre Unterhaltung fortsetzen, steht unter „Agenten &amp; MCP“.</source>
        <translation>Whether those agents restart and resume their conversation is set under “Agents &amp; MCP”.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="95"/>
        <source>Quake-Modus</source>
        <translation>Quake mode</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="97"/>
        <source>Globaler Hotkey Strg+^ blendet QTmux überall ein und aus.</source>
        <translation>Global hotkey Ctrl+^ shows and hides QTmux from anywhere.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="132"/>
        <source>Jetzt nach Updates suchen</source>
        <translation>Check for updates now</translation>
    </message>
    <message>
        <source>QTmux startet mit einer einzelnen, leeren Session. Der zuletzt gespeicherte Stand bleibt dabei erhalten — er wird beim Beenden nicht überschrieben und ist wieder da, sobald hier erneut wiederhergestellt wird.</source>
        <translation type="vanished">QTmux starts with a single, empty session. The last saved state is kept — it is not overwritten on exit and returns as soon as you choose to restore again.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="61"/>
        <source>Fenster, Panes und deren Arbeitsverzeichnisse kommen zurück, die Terminals starten aber leer. Der gespeicherte Verlauf bleibt liegen und wird bei „Alles“ wieder angezeigt.</source>
        <translation>Windows, panes and their working directories return, but the terminals start empty. The saved history is kept and is shown again with “Everything”.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="64"/>
        <source>Fenster, Panes und Arbeitsverzeichnisse kommen zurück, dazu der farbige Verlauf jedes Panes.</source>
        <translation>Windows, panes and working directories return, along with each pane’s colored history.</translation>
    </message>
    <message>
        <source>Ob die Agenten in den Panes dabei erneut starten, steht unter „Agenten &amp; MCP“.</source>
        <translation type="vanished">Whether the agents inside the panes start again is configured under “Agents &amp; MCP”.</translation>
    </message>
    <message>
        <source>Quake-Modus: per globalem Hotkey ein-/ausblenden</source>
        <translation type="vanished">Quake mode: toggle via global hotkey</translation>
    </message>
    <message>
        <source>Globaler Hotkey: Strg+^ (blendet QTmux überall ein/aus)</source>
        <translation type="vanished">Global hotkey: Ctrl+` (toggles QTmux from anywhere)</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="98"/>
        <source>Derzeit nur unter macOS verfügbar.</source>
        <translation>Currently available on macOS only.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="114"/>
        <source>Aktualisierung</source>
        <translation>Updates</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="116"/>
        <source>Beim Start automatisch nach Updates suchen</source>
        <translation>Check for updates automatically at startup</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="117"/>
        <source>Höchstens einmal am Tag und still: Gibt es nichts Neues oder ist der Server nicht erreichbar, passiert gar nichts. Über das Hilfe-Menü lässt sich jederzeit von Hand suchen.</source>
        <translation>At most once a day and silently: if there is nothing new, or the server cannot be reached, nothing happens at all. You can always check manually from the Help menu.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="127"/>
        <source>Jetzt suchen</source>
        <translation>Check now</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="129"/>
        <source>Installiert ist Version %1.</source>
        <translation>Installed version is %1.</translation>
    </message>
    <message>
        <source>Suchen …</source>
        <translation type="obsolete">Search …</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="146"/>
        <source>Energie</source>
        <translation>Power</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="148"/>
        <source>Ruhezustand verhindern, solange Agenten arbeiten</source>
        <translation>Prevent sleep while agents are working</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="150"/>
        <source>Der Rechner bleibt wach, solange mindestens eine Session „beschäftigt“ meldet — und nur dann. Wartet ein Agent auf eine Antwort von dir, darf der Rechner schlafen. Der Bildschirm wird nicht wachgehalten.</source>
        <translation>The computer stays awake while at least one session reports “busy” — and only then. If an agent is waiting for your answer, the computer may sleep. The display is not kept awake.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="154"/>
        <source>Auf dieser Plattform noch nicht verfügbar.</source>
        <translation>Not available on this platform yet.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="164"/>
        <source>Zustand</source>
        <translation>State</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="166"/>
        <source>Aktiv — der Ruhezustand ist gerade gesperrt.</source>
        <translation>Active — sleep is currently blocked.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="167"/>
        <source>Zurzeit nicht gesperrt: keine Session arbeitet.</source>
        <translation>Not blocked right now: no session is working.</translation>
    </message>
</context>
<context>
    <name>CatEingabe</name>
    <message>
        <source>Eingabe</source>
        <translation type="vanished">Input</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatEingabe.qml" line="11"/>
        <source>Eingabe &amp; Zwischenablage</source>
        <translation>Input &amp; Clipboard</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatEingabe.qml" line="12"/>
        <source>Auswahl und Zwischenablage.</source>
        <translation>Selection and clipboard.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatEingabe.qml" line="18"/>
        <source>Zwischenablage</source>
        <translation>Clipboard</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatEingabe.qml" line="20"/>
        <source>Auswahl automatisch kopieren</source>
        <translation>Copy on select</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatEingabe.qml" line="21"/>
        <source>PuTTY-Stil: markierter Text landet sofort in der Zwischenablage.</source>
        <translation>PuTTY style: selected text goes to the clipboard right away.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatEingabe.qml" line="28"/>
        <source>Rechtsklick fügt ein</source>
        <translation>Right-click pastes</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatEingabe.qml" line="29"/>
        <source>Statt des Kontextmenüs — dieses erreichst du dann über die Menüleiste.</source>
        <translation>Instead of the context menu — you can still reach it from the menu bar.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatEingabe.qml" line="36"/>
        <source>Vor mehrzeiligem Einfügen warnen</source>
        <translation>Warn before multiline paste</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatEingabe.qml" line="37"/>
        <source>Mehrere Zeilen wirken in einer Shell wie mehrere abgeschickte Befehle.</source>
        <translation>In a shell, multiple lines act like multiple submitted commands.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatEingabe.qml" line="50"/>
        <source>Maus</source>
        <translation>Mouse</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatEingabe.qml" line="52"/>
        <source>Mausrad in Vollbild-Anwendungen</source>
        <translation>Mouse wheel in full-screen applications</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatEingabe.qml" line="53"/>
        <source>Vollbild-Anwendungen (Agenten, less, vim) zeichnen ihren Verlauf selbst — der Verlauf von QTmux ist dort leer. Greift die Anwendung die Maus nicht, kann das Rad nur wirken, wenn QTmux daraus Pfeiltasten macht. „Nur auf Anforderung“ tut das ausschließlich, wenn die Anwendung es verlangt (Codex tut das); „Immer“ deckt zusätzlich Anzeigeprogramme wie less und man ab, bewegt in vim aber den Cursor statt zu scrollen.</source>
        <translation>Full-screen applications (agents, less, vim) draw their own history — QTmux’s own scrollback is empty while they run. If such an application does not grab the mouse, the wheel can only do something when QTmux turns it into arrow keys. “Only when requested” does that solely when the application asks for it (Codex does); “Always” additionally covers pagers such as less and man, but in vim it moves the cursor instead of scrolling.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatEingabe.qml" line="60"/>
        <source>Nur auf Anforderung</source>
        <translation>Only when requested</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatEingabe.qml" line="60"/>
        <source>Immer</source>
        <translation>Always</translation>
    </message>
</context>
<context>
    <name>CatErscheinungsbild</name>
    <message>
        <location filename="../qml/prefs/CatErscheinungsbild.qml" line="13"/>
        <source>Erscheinungsbild</source>
        <translation>Appearance</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatErscheinungsbild.qml" line="14"/>
        <source>Farbschemata für Dunkel und Hell. Das aktive Schema färbt die gesamte App.</source>
        <translation>Color schemes for dark and light. The active scheme colors the entire app.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatErscheinungsbild.qml" line="39"/>
        <source>Farbschemata</source>
        <translation>Colour schemes</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatErscheinungsbild.qml" line="43"/>
        <source>Farbschema (Dunkel)</source>
        <translation>Color scheme (dark)</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatErscheinungsbild.qml" line="44"/>
        <source>Gilt im Dunkel-Modus — für Terminal UND App-Chrome.</source>
        <translation>Applies in dark mode — to the terminal AND the app chrome.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatErscheinungsbild.qml" line="53"/>
        <source>Importieren …</source>
        <translation>Import …</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatErscheinungsbild.qml" line="59"/>
        <source>Farben (Dunkel)</source>
        <translation>Colours (dark)</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatErscheinungsbild.qml" line="60"/>
        <location filename="../qml/prefs/CatErscheinungsbild.qml" line="76"/>
        <source>Die 16 ANSI-Farben des gewählten Schemas.</source>
        <translation>The 16 ANSI colours of the selected scheme.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatErscheinungsbild.qml" line="64"/>
        <source>Farbschema (Hell)</source>
        <translation>Color scheme (light)</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatErscheinungsbild.qml" line="65"/>
        <source>Gilt im Hell-Modus. Import landet immer im passenden Slot.</source>
        <translation>Applies in light mode. An import always lands in the matching slot.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatErscheinungsbild.qml" line="75"/>
        <source>Farben (Hell)</source>
        <translation>Colours (light)</translation>
    </message>
</context>
<context>
    <name>CatErweiterungen</name>
    <message>
        <location filename="../qml/prefs/CatErweiterungen.qml" line="11"/>
        <source>Erweiterungen</source>
        <translation>Extensions</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatErweiterungen.qml" line="12"/>
        <source>Geladene Plugins und die Backend-Typen, die sie bereitstellen.</source>
        <translation>Loaded plugins and the backend types they provide.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatErweiterungen.qml" line="56"/>
        <source>Backend-Typen: %1</source>
        <translation>Backend types: %1</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatErweiterungen.qml" line="56"/>
        <source>(keine)</source>
        <translation>(none)</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatErweiterungen.qml" line="80"/>
        <source>Keine Plugins geladen. Plugins liegen in „&lt;App&gt;/plugins“ bzw. „&lt;AppData&gt;/QTmux/plugins“ und werden beim Start eingesammelt.</source>
        <translation>No plugins loaded. Plugins live in “&lt;App&gt;/plugins” or “&lt;AppData&gt;/QTmux/plugins” and are collected at startup.</translation>
    </message>
</context>
<context>
    <name>CatHotkeys</name>
    <message>
        <location filename="../qml/prefs/CatHotkeys.qml" line="17"/>
        <source>Tastenkürzel</source>
        <translation>Keyboard shortcuts</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatHotkeys.qml" line="18"/>
        <source>Klick auf ein Kürzel nimmt eine neue Belegung auf. „Standard“ erscheint nur bei Abweichung.</source>
        <translation>Clicking a shortcut records a new binding. “Default” appears only when it differs.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatHotkeys.qml" line="48"/>
        <source>Sitzungen</source>
        <translation>Sessions</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatHotkeys.qml" line="49"/>
        <source>Panes</source>
        <translation>Panes</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatHotkeys.qml" line="50"/>
        <source>Verbindungen</source>
        <translation>Connections</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatHotkeys.qml" line="51"/>
        <source>Ansicht &amp; App</source>
        <translation>View &amp; App</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatHotkeys.qml" line="63"/>
        <source>Weitere</source>
        <translation>Other</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatHotkeys.qml" line="126"/>
        <source>(keins)</source>
        <translation>(none)</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatHotkeys.qml" line="132"/>
        <source>Standard</source>
        <translation>Default</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatHotkeys.qml" line="147"/>
        <source>Tasten drücken …</source>
        <translation>Press keys …</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatHotkeys.qml" line="155"/>
        <source>Leeren</source>
        <translation>Clear</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatHotkeys.qml" line="162"/>
        <source>Abbrechen</source>
        <translation>Cancel</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatHotkeys.qml" line="176"/>
        <source>Bereits belegt von: %1</source>
        <translation>Already assigned to: %1</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatHotkeys.qml" line="184"/>
        <source>Trotzdem zuweisen</source>
        <translation>Assign anyway</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatHotkeys.qml" line="184"/>
        <source>Zuweisen</source>
        <translation>Assign</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatHotkeys.qml" line="195"/>
        <source>Alle Kürzel zurücksetzen</source>
        <translation>Reset all shortcuts</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatHotkeys.qml" line="200"/>
        <source>Mehrere nacheinander gedrückte Akkorde ergeben eine Tastenfolge (max. 4). Esc bricht ab, Eingabe bestätigt.</source>
        <translation>Several chords pressed in sequence form a key sequence (max. 4). Esc cancels, Enter confirms.</translation>
    </message>
</context>
<context>
    <name>CatTerminal</name>
    <message>
        <source>Terminal</source>
        <translation type="vanished">Terminal</translation>
    </message>
    <message>
        <source>Schrift, Ligaturen und Rendering des Terminals.</source>
        <translation type="vanished">Font, ligatures and rendering of the terminal.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatTerminal.qml" line="21"/>
        <source>Schriftart</source>
        <translation>Font</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatTerminal.qml" line="32"/>
        <source>Schriftgröße</source>
        <translation>Font size</translation>
    </message>
    <message>
        <source>Ligaturen</source>
        <translation type="vanished">Ligatures</translation>
    </message>
    <message>
        <source>Programmier-Ligaturen (z. B. FiraCode)</source>
        <translation type="vanished">Programming ligatures (e.g. FiraCode)</translation>
    </message>
    <message>
        <source>Rendering</source>
        <translation type="vanished">Rendering</translation>
    </message>
    <message>
        <source>GPU-Glyph-Atlas (schneller; aus = QPainter-Fallback)</source>
        <translation type="vanished">GPU glyph atlas (faster; off = QPainter fallback)</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatTerminal.qml" line="11"/>
        <source>Darstellung &amp; Shell</source>
        <translation>Appearance &amp; shell</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatTerminal.qml" line="12"/>
        <source>Schrift, Ligaturen, Rendering und die Shell neuer Sessions.</source>
        <translation>Font, ligatures, rendering and the shell of new sessions.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatTerminal.qml" line="19"/>
        <source>Schrift</source>
        <translation>Font</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatTerminal.qml" line="22"/>
        <source>Nur Monospace-Schriften — Proportionalschrift zerlegt das Zellraster.</source>
        <translation>Monospace fonts only — a proportional font breaks the cell grid.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatTerminal.qml" line="33"/>
        <source>Wirkt auf alle Sessions; einzelne Fenster zoomst du mit Strg/Cmd +/−.</source>
        <translation>Applies to all sessions; zoom individual windows with Ctrl/Cmd +/−.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatTerminal.qml" line="41"/>
        <source>Programmier-Ligaturen</source>
        <translation>Programming ligatures</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatTerminal.qml" line="42"/>
        <source>Verbindet Zeichenfolgen wie != oder =&gt; zu einem Glyph (z. B. FiraCode).</source>
        <translation>Joins sequences like != or =&gt; into a single glyph (e.g. FiraCode).</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatTerminal.qml" line="49"/>
        <source>GPU-Glyph-Atlas</source>
        <translation>GPU glyph atlas</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatTerminal.qml" line="50"/>
        <source>Schneller; aus = QPainter-Fallback. Bei Darstellungsfehlern hilft „aus“.</source>
        <translation>Faster; off = QPainter fallback. If rendering looks wrong, try “off”.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatTerminal.qml" line="61"/>
        <source>Shell</source>
        <translation>Shell</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatTerminal.qml" line="64"/>
        <source>Standard-Shell</source>
        <translation>Default Shell</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatTerminal.qml" line="65"/>
        <source>Gilt für neue Sessions. Dieselbe Wahl steckt im „+“-Menü der Leiste.</source>
        <translation>Applies to new sessions. The same choice sits in the “+” menu of the bar.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatTerminal.qml" line="91"/>
        <source>Vorschau</source>
        <translation>Preview</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatTerminal.qml" line="121"/>
        <source>$ qtmux --version   # Beispieltext in der gewählten Schrift</source>
        <translation>$ qtmux --version   # sample text in the chosen font</translation>
    </message>
</context>
<context>
    <name>CatVault</name>
    <message>
        <location filename="../qml/prefs/CatVault.qml" line="12"/>
        <source>Secrets-Vault</source>
        <translation>Secrets Vault</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatVault.qml" line="13"/>
        <source>Verschlüsselter Speicher für Passwörter, Passphrasen und Tokens.</source>
        <translation>Encrypted storage for passwords, passphrases and tokens.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatVault.qml" line="26"/>
        <source>Der Vault ist gesperrt. Master-Passwort eingeben:</source>
        <translation>The vault is locked. Enter the master password:</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatVault.qml" line="27"/>
        <source>Noch kein Vault vorhanden. Lege ein Master-Passwort fest:</source>
        <translation>No vault yet. Set a master password:</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatVault.qml" line="34"/>
        <source>Master-Passwort</source>
        <translation>Master password</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatVault.qml" line="42"/>
        <source>Master-Passwort bestätigen</source>
        <translation>Confirm master password</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatVault.qml" line="52"/>
        <source>Entsperren</source>
        <translation>Unlock</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatVault.qml" line="52"/>
        <source>Vault anlegen</source>
        <translation>Create vault</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatVault.qml" line="57"/>
        <source>Falsches Master-Passwort.</source>
        <translation>Wrong master password.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatVault.qml" line="59"/>
        <source>Bitte ein Passwort eingeben.</source>
        <translation>Please enter a password.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatVault.qml" line="61"/>
        <source>Die Passwörter stimmen nicht überein.</source>
        <translation>The passwords do not match.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatVault.qml" line="63"/>
        <source>Der Vault konnte nicht angelegt werden.</source>
        <translation>The vault could not be created.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatVault.qml" line="69"/>
        <source>Der Vault speichert Geheimnisse (Passwörter, Passphrasen, Tokens) verschlüsselt hinter dem Master-Passwort. Das Master-Passwort wird nicht gespeichert und kann nicht wiederhergestellt werden.</source>
        <translation>The vault stores secrets (passwords, passphrases, tokens) encrypted behind the master password. The master password is not stored and cannot be recovered.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatVault.qml" line="81"/>
        <source>Gespeicherte Geheimnisse</source>
        <translation>Stored secrets</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatVault.qml" line="82"/>
        <source>Hinzufügen</source>
        <translation>Add</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatVault.qml" line="117"/>
        <source>Verbergen</source>
        <translation>Hide</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatVault.qml" line="117"/>
        <source>Anzeigen</source>
        <translation>Show</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatVault.qml" line="121"/>
        <source>In Zwischenablage kopieren</source>
        <translation>Copy to clipboard</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatVault.qml" line="122"/>
        <source>Bearbeiten</source>
        <translation>Edit</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatVault.qml" line="123"/>
        <source>Löschen</source>
        <translation>Delete</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatVault.qml" line="129"/>
        <source>Noch keine Geheimnisse. Mit „Hinzufügen“ eines anlegen.</source>
        <translation>No secrets yet. Add one with “Add”.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatVault.qml" line="134"/>
        <source>Master-Passwort ändern …</source>
        <translation>Change master password …</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatVault.qml" line="136"/>
        <source>Sperren</source>
        <translation>Lock</translation>
    </message>
</context>
<context>
    <name>CatVerbindungen</name>
    <message>
        <location filename="../qml/prefs/CatVerbindungen.qml" line="12"/>
        <source>Verbindungen</source>
        <translation>Connections</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatVerbindungen.qml" line="13"/>
        <source>Wiederverwendbare Profile für SSH, seriell und Plugin-Backends.</source>
        <translation>Reusable profiles for SSH, serial and plugin backends.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatVerbindungen.qml" line="25"/>
        <source>Gespeicherte Verbindungsprofile</source>
        <translation>Saved connection profiles</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatVerbindungen.qml" line="31"/>
        <source>Neu …</source>
        <translation>New …</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatVerbindungen.qml" line="101"/>
        <source>Verbinden</source>
        <translation>Connect</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatVerbindungen.qml" line="105"/>
        <source>SFTP</source>
        <translation>SFTP</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatVerbindungen.qml" line="111"/>
        <source>Bearbeiten</source>
        <translation>Edit</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatVerbindungen.qml" line="116"/>
        <source>Löschen</source>
        <translation>Delete</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatVerbindungen.qml" line="129"/>
        <source>Noch keine Profile. Lege mit „Neu …“ eine wiederverwendbare Verbindung an.</source>
        <translation>No profiles yet. Create a reusable connection with “New …”.</translation>
    </message>
</context>
<context>
    <name>Main</name>
    <message>
        <location filename="../qml/Main.qml" line="664"/>
        <location filename="../qml/Main.qml" line="1760"/>
        <location filename="../qml/Main.qml" line="2484"/>
        <source>Neue Session</source>
        <translation>New Session</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="666"/>
        <location filename="../qml/Main.qml" line="1779"/>
        <location filename="../qml/Main.qml" line="2357"/>
        <location filename="../qml/Main.qml" line="2491"/>
        <source>Session schließen</source>
        <translation>Close Session</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1786"/>
        <location filename="../qml/Main.qml" line="2810"/>
        <source>Helles Design</source>
        <translation>Light Theme</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1786"/>
        <location filename="../qml/Main.qml" line="2810"/>
        <source>Dunkles Design</source>
        <translation>Dark Theme</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="689"/>
        <location filename="../qml/Main.qml" line="1793"/>
        <location filename="../qml/Main.qml" line="2571"/>
        <source>Beenden</source>
        <translation>Quit</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1801"/>
        <location filename="../qml/Main.qml" line="2512"/>
        <location filename="../qml/Main.qml" line="2822"/>
        <source>Einstellungen …</source>
        <translation>Settings …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1812"/>
        <location filename="../qml/Main.qml" line="2495"/>
        <source>Schrift vergrößern</source>
        <translation>Increase font size</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1819"/>
        <location filename="../qml/Main.qml" line="2496"/>
        <source>Schrift verkleinern</source>
        <translation>Decrease font size</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="683"/>
        <location filename="../qml/Main.qml" line="1826"/>
        <location filename="../qml/Main.qml" line="2497"/>
        <source>Schriftgröße zurücksetzen</source>
        <translation>Reset font size</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="677"/>
        <location filename="../qml/Main.qml" line="1875"/>
        <location filename="../qml/Main.qml" line="2500"/>
        <source>Eingabe an alle Sessions</source>
        <translation>Send input to all sessions</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="673"/>
        <location filename="../qml/Main.qml" line="1912"/>
        <location filename="../qml/Main.qml" line="2366"/>
        <location filename="../qml/Main.qml" line="2492"/>
        <source>Nebeneinander teilen</source>
        <translation>Split side by side</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="674"/>
        <location filename="../qml/Main.qml" line="1919"/>
        <location filename="../qml/Main.qml" line="2371"/>
        <location filename="../qml/Main.qml" line="2493"/>
        <source>Untereinander teilen</source>
        <translation>Split top and bottom</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="667"/>
        <location filename="../qml/Main.qml" line="1926"/>
        <location filename="../qml/Main.qml" line="2494"/>
        <source>Pane schließen</source>
        <translation>Close pane</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1956"/>
        <source>Befehlspalette …</source>
        <translation>Command palette …</translation>
    </message>
    <message>
        <source>Befehlspalette (Strg/Cmd+K)</source>
        <translation type="vanished">Command palette (Ctrl/Cmd+K)</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2380"/>
        <source>Broadcast-Eingabe: an (an alle Sessions)</source>
        <translation>Broadcast input: on (to all sessions)</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2381"/>
        <source>Eingabe an alle Sessions (Broadcast)</source>
        <translation>Send input to all sessions (broadcast)</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2606"/>
        <source>Verbinden: %1</source>
        <translation>Connect: %1</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2532"/>
        <source>Auswahl automatisch kopieren</source>
        <translation>Copy on select</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2533"/>
        <source>Rechtsklick fügt ein</source>
        <translation>Right-click pastes</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2534"/>
        <source>Vor mehrzeiligem Einfügen warnen</source>
        <translation>Warn before multiline paste</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2554"/>
        <source>Design: Wie System</source>
        <translation>Theme: Follow System</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2555"/>
        <source>Design: Hell</source>
        <translation>Theme: Light</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2556"/>
        <source>Design: Dunkel</source>
        <translation>Theme: Dark</translation>
    </message>
    <message>
        <source>Verbindungen</source>
        <translation type="vanished">Connections</translation>
    </message>
    <message>
        <source>Gespeicherte Verbindungsprofile</source>
        <translation type="vanished">Saved connection profiles</translation>
    </message>
    <message>
        <source>Neu …</source>
        <translation type="vanished">New …</translation>
    </message>
    <message>
        <source>Verbinden</source>
        <translation type="vanished">Connect</translation>
    </message>
    <message>
        <source>SFTP</source>
        <translation type="vanished">SFTP</translation>
    </message>
    <message>
        <source>Löschen</source>
        <translation type="vanished">Delete</translation>
    </message>
    <message>
        <source>Noch keine Profile. Lege mit „Neu …“ eine wiederverwendbare Verbindung an.</source>
        <translation type="vanished">No profiles yet. Create a reusable connection with “New …”.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3910"/>
        <source>Verbindungsprofil</source>
        <translation>Connection Profile</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3970"/>
        <location filename="../qml/Main.qml" line="4268"/>
        <source>Name</source>
        <translation>Name</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3971"/>
        <source>z. B. Prod-Server</source>
        <translation>e.g. Prod Server</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3972"/>
        <source>Typ</source>
        <translation>Type</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3995"/>
        <source>Passwort (Vault)</source>
        <translation>Password (vault)</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4005"/>
        <source>(keines)</source>
        <translation>(none)</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4020"/>
        <source>Vault gesperrt – beim Verbinden entsperren, sonst kein Auto-Fill.</source>
        <translation>Vault locked – unlock before connecting, otherwise no auto-fill.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4033"/>
        <source>Programm</source>
        <translation>Program</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4034"/>
        <source>leer = Standard-Shell</source>
        <translation>empty = default shell</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4035"/>
        <source>Arbeitsverzeichnis</source>
        <translation>Working directory</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4036"/>
        <source>leer = Home</source>
        <translation>empty = home</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4067"/>
        <source>Befehle nach Verbindung (eine pro Zeile)</source>
        <translation>Commands after connect (one per line)</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4086"/>
        <source>z. B. cd ~/projekt
source .venv/bin/activate</source>
        <translation>e.g. cd ~/project
source .venv/bin/activate</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4092"/>
        <source>Werden gesendet, sobald die Shell bereit ist (Shell-Integration: am ersten Prompt, sonst kurz nach Verbindungsaufbau). Geeignet für key-/agent-authentifizierte Verbindungen.</source>
        <translation>Sent as soon as the shell is ready (with shell integration: at the first prompt, otherwise shortly after connect). Suited to key/agent-authenticated connections.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4110"/>
        <source>Zielordner für den Download</source>
        <translation>Download destination folder</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4115"/>
        <source>Datei zum Hochladen</source>
        <translation>File to upload</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4123"/>
        <source>SFTP – %1</source>
        <translation>SFTP – %1</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4138"/>
        <source>Übergeordnetes Verzeichnis</source>
        <translation>Parent directory</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4157"/>
        <source>Aktualisieren</source>
        <translation>Refresh</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4224"/>
        <source>Herunterladen</source>
        <translation>Download</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4235"/>
        <source>Hochladen …</source>
        <translation>Upload …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="681"/>
        <source>Secrets-Vault</source>
        <translation>Secrets Vault</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="151"/>
        <location filename="../qml/Main.qml" line="205"/>
        <location filename="../qml/Main.qml" line="217"/>
        <location filename="../qml/Main.qml" line="223"/>
        <source>Unbekannte windowId.</source>
        <translation>Unknown windowId.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="157"/>
        <source>Kein Layout vorhanden.</source>
        <translation>No layout available.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="228"/>
        <source>Letztes Fenster: Schließen würde QTmux beenden — über MCP nicht möglich. Nutze close_pane/close_session oder beende die App in der Oberfläche.</source>
        <translation>Last window: closing it would quit QTmux — not possible via MCP. Use close_pane/close_session, or quit the app from the user interface.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="240"/>
        <location filename="../qml/Main.qml" line="248"/>
        <location filename="../qml/Main.qml" line="255"/>
        <source>Unbekannte paneId.</source>
        <translation>Unknown paneId.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="261"/>
        <source>assign_session entfällt im Window-Modell — nutze focus_session bzw. focus_window.</source>
        <translation>assign_session is obsolete in the window model — use focus_session or focus_window.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="265"/>
        <source>Unbekanntes Profil.</source>
        <translation>Unknown profile.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="431"/>
        <source>Der Bildschirm ist bereits leer.</source>
        <translation>The screen is already empty.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="438"/>
        <source>Terminal-Eingabe zurückgesetzt (Maus/Einfügen).</source>
        <translation>Terminal input reset (mouse/paste).</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="665"/>
        <location filename="../qml/Main.qml" line="1768"/>
        <location filename="../qml/Main.qml" line="2483"/>
        <source>Neues Fenster</source>
        <translation>New Window</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="668"/>
        <location filename="../qml/Main.qml" line="1933"/>
        <location filename="../qml/Main.qml" line="2527"/>
        <source>Nächstes Pane</source>
        <translation>Next Pane</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="669"/>
        <location filename="../qml/Main.qml" line="1940"/>
        <location filename="../qml/Main.qml" line="2528"/>
        <source>Vorheriges Pane</source>
        <translation>Previous Pane</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="670"/>
        <location filename="../qml/Main.qml" line="1947"/>
        <location filename="../qml/Main.qml" line="2529"/>
        <source>Pane zoomen</source>
        <translation>Zoom Pane</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="671"/>
        <location filename="../qml/Main.qml" line="1966"/>
        <location filename="../qml/Main.qml" line="2525"/>
        <source>Nächste Session</source>
        <translation>Next session</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="672"/>
        <location filename="../qml/Main.qml" line="1973"/>
        <location filename="../qml/Main.qml" line="2526"/>
        <source>Vorige Session</source>
        <translation>Previous session</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="676"/>
        <location filename="../qml/Main.qml" line="2524"/>
        <source>Suchen (Scrollback)</source>
        <translation>Search (Scrollback)</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="678"/>
        <source>Neue SSH-Verbindung</source>
        <translation>New SSH connection</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="679"/>
        <source>Neue serielle Verbindung</source>
        <translation>New serial connection</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="680"/>
        <source>Verbindungen verwalten</source>
        <translation>Manage connections</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="684"/>
        <location filename="../qml/Main.qml" line="1859"/>
        <location filename="../qml/Main.qml" line="2498"/>
        <source>Bildschirm leeren</source>
        <translation>Clear screen</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="685"/>
        <location filename="../qml/Main.qml" line="1867"/>
        <location filename="../qml/Main.qml" line="2499"/>
        <source>Terminal-Eingabe zurücksetzen</source>
        <translation>Reset terminal input</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="863"/>
        <source>Fenster %1</source>
        <translation>Window %1</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="883"/>
        <source>Keine aktive Session.</source>
        <translation>No active session.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="893"/>
        <source>Verzeichnis lässt sich nicht öffnen: %1</source>
        <translation>Cannot open directory: %1</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="957"/>
        <source>braucht Aufmerksamkeit</source>
        <translation>needs attention</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="958"/>
        <source>untätig</source>
        <translation>idle</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="959"/>
        <source>arbeitet</source>
        <translation>working</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="960"/>
        <source>wartet auf Eingabe</source>
        <translation>waiting for input</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="962"/>
        <source>beendet</source>
        <translation>closed</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="969"/>
        <source>seit %1 s</source>
        <translation>for %1 s</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="970"/>
        <source>seit %1 min</source>
        <translation>for %1 min</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="971"/>
        <source>seit %1 h</source>
        <translation>for %1 h</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1773"/>
        <source>Kein freier MCP-Port für eine neue Instanz gefunden.</source>
        <translation>No free MCP port found for a new instance.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1774"/>
        <source>Neues Fenster gestartet (MCP-Port %1).</source>
        <translation>New window started (MCP port %1).</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1836"/>
        <source>Seitenleiste</source>
        <translation>Sidebar</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1849"/>
        <location filename="../qml/Main.qml" line="2509"/>
        <source>Statusleiste anzeigen</source>
        <translation>Show status bar</translation>
    </message>
    <message>
        <source>Suchen …</source>
        <translation type="vanished">Search …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1947"/>
        <location filename="../qml/Main.qml" line="2529"/>
        <source>Pane-Zoom aufheben</source>
        <translation>Unzoom Pane</translation>
    </message>
    <message>
        <source>MCP-Server stoppen (127.0.0.1:%1)</source>
        <translation type="vanished">Stop MCP server (127.0.0.1:%1)</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2011"/>
        <source>MCP-Server starten</source>
        <translation>Start MCP server</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2022"/>
        <location filename="../qml/Main.qml" line="2568"/>
        <source>Nach Updates suchen …</source>
        <translation>Check for Updates …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2038"/>
        <location filename="../qml/Main.qml" line="2562"/>
        <source>Alles auswählen</source>
        <translation>Select all</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2056"/>
        <location filename="../qml/Main.qml" line="2565"/>
        <source>Agent-Ereignisse …</source>
        <translation>Agent events …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2062"/>
        <source>Agenten-Einstellungen …</source>
        <translation>Agent settings …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2071"/>
        <location filename="../qml/Main.qml" line="2566"/>
        <source>Dokumentation</source>
        <translation>Documentation</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2077"/>
        <location filename="../qml/Main.qml" line="2567"/>
        <source>Tastenkürzel-Übersicht</source>
        <translation>Keyboard shortcuts</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2084"/>
        <source>Minimieren</source>
        <translation>Minimize</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2091"/>
        <source>Zoomen</source>
        <translation>Zoom</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2204"/>
        <source>keine Session</source>
        <translation>no session</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2210"/>
        <source>Klick: Fokus ins aktive Pane</source>
        <translation>Click: focus the active pane</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2221"/>
        <source>%1 Sessions</source>
        <translation>%1 sessions</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2222"/>
        <source>%1 wartet</source>
        <translation>%1 waiting</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2223"/>
        <source>%1 Fehler</source>
        <translation>%1 with errors</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2226"/>
        <source>Sessions insgesamt, wartend, mit Fehler</source>
        <translation>Sessions total, waiting, with errors</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2236"/>
        <source>Rastergröße des aktiven Panes: Spalten × Zeilen</source>
        <translation>Grid size of the active pane: columns × rows</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2243"/>
        <source>Kodierung des Terminals</source>
        <translation>Terminal encoding</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2254"/>
        <source>MCP :%1</source>
        <translation>MCP :%1</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2252"/>
        <source>MCP aus</source>
        <translation>MCP off</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2265"/>
        <source>Klick: MCP-Server stoppen · Rechtsklick: Einstellungen</source>
        <translation>Click: stop the MCP server · right-click: settings</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2260"/>
        <source>Klick: MCP-Server starten · Rechtsklick: Einstellungen</source>
        <translation>Click: start the MCP server · right-click: settings</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2010"/>
        <source>MCP-Server stoppen (%1:%2)</source>
        <translation>Stop MCP server (%1:%2)</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2253"/>
        <source>MCP LAN :%1</source>
        <translation>MCP LAN :%1</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2262"/>
        <source>Erreichbar auf %1:%2 — Anfragen brauchen ein Token.
Klick: MCP-Server stoppen · Rechtsklick: Einstellungen</source>
        <translation>Reachable at %1:%2 — requests need a token.
Click: stop MCP server · Right-click: settings</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2272"/>
        <source>Vault offen</source>
        <translation>Vault unlocked</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2272"/>
        <source>Vault zu</source>
        <translation>Vault locked</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2274"/>
        <source>Klick: Vault verwalten</source>
        <translation>Click: manage the vault</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2281"/>
        <source>Broadcast</source>
        <translation>Broadcast</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2283"/>
        <source>Klick: Eingabe an alle Sessions umschalten</source>
        <translation>Click: toggle input to all sessions</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2290"/>
        <source>Klick: Design umschalten · Rechtsklick: Erscheinungsbild</source>
        <translation>Click: toggle theme · right-click: appearance</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2485"/>
        <source>In die Warteschlange einreihen …</source>
        <translation>Add to queue …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2504"/>
        <source>Seitenleiste ausklappen</source>
        <translation>Expand sidebar</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2505"/>
        <source>Seitenleiste einklappen</source>
        <translation>Collapse sidebar</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2508"/>
        <source>Statusleiste ausblenden</source>
        <translation>Hide status bar</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2518"/>
        <source>MCP-Netzzugang: eingeschaltet (%1) …</source>
        <translation>MCP network access: on (%1) …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2519"/>
        <source>MCP im Netzwerk erreichbar machen …</source>
        <translation>Make MCP reachable over the network …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2530"/>
        <source>Ligaturen umschalten</source>
        <translation>Toggle Ligatures</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2531"/>
        <source>GPU-Rendering umschalten</source>
        <translation>Toggle GPU Rendering</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2535"/>
        <source>Mausrad in Vollbild-Anwendungen: nur auf Anforderung</source>
        <translation>Mouse wheel in full-screen applications: only when requested</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2536"/>
        <source>Mausrad in Vollbild-Anwendungen: immer</source>
        <translation>Mouse wheel in full-screen applications: always</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2538"/>
        <source>Sessions wiederherstellen: gar nicht</source>
        <translation>Restore sessions: not at all</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2539"/>
        <source>Sessions wiederherstellen: ohne Verlauf</source>
        <translation>Restore sessions: without history</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2540"/>
        <source>Sessions wiederherstellen: alles</source>
        <translation>Restore sessions: everything</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2541"/>
        <source>Ruhezustand wieder zulassen</source>
        <translation>Allow sleep again</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2541"/>
        <source>Ruhezustand verhindern, solange Agenten arbeiten</source>
        <translation>Prevent sleep while agents are working</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2544"/>
        <location filename="../qml/Main.qml" line="4608"/>
        <source>Arbeitsverzeichnis öffnen</source>
        <translation>Open working directory</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2545"/>
        <location filename="../qml/Main.qml" line="4614"/>
        <source>Pfad kopieren</source>
        <translation>Copy path</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2547"/>
        <source>Diese Session hat kein Arbeitsverzeichnis.</source>
        <translation>This session has no working directory.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2548"/>
        <location filename="../qml/Main.qml" line="4619"/>
        <source>Pfad kopiert: %1</source>
        <translation>Path copied: %1</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2549"/>
        <source>Agenten beim Start wiederherstellen</source>
        <translation>Restore agents on start</translation>
    </message>
    <message>
        <source>Agenten-Unterhaltung fortsetzen</source>
        <translation type="vanished">Continue agent conversation</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2557"/>
        <source>Sprache: Deutsch</source>
        <translation>Language: German</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2558"/>
        <source>Sprache: English</source>
        <translation>Language: English</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2575"/>
        <source>Quake-Modus umschalten</source>
        <translation>Toggle Quake Mode</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2580"/>
        <source>Neue Plugin-Session</source>
        <translation>New plugin session</translation>
    </message>
    <message>
        <source>Aktive Session</source>
        <translation type="vanished">Active session</translation>
    </message>
    <message>
        <source>Session gruppieren …</source>
        <translation type="vanished">Group session …</translation>
    </message>
    <message>
        <source>Session zu Gruppe: %1</source>
        <translation type="vanished">Add session to group: %1</translation>
    </message>
    <message>
        <source>Session aus Gruppe nehmen</source>
        <translation type="vanished">Remove session from group</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2635"/>
        <source>Gruppe umbenennen: %1 …</source>
        <translation>Rename group: %1 …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2638"/>
        <source>Gruppe auflösen: %1</source>
        <translation>Dissolve group: %1</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2848"/>
        <source>&amp;Datei</source>
        <translation>&amp;File</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2881"/>
        <source>&amp;Bearbeiten</source>
        <translation>&amp;Edit</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2895"/>
        <source>&amp;Ansicht</source>
        <translation>&amp;View</translation>
    </message>
    <message>
        <source>&amp;Sprache</source>
        <translation type="vanished">&amp;Language</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2975"/>
        <source>A&amp;gent</source>
        <translation>A&amp;gent</translation>
    </message>
    <message>
        <source>Agent-S&amp;teuerung</source>
        <translation type="vanished">Agent &amp;Control</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2998"/>
        <source>&amp;Hilfe</source>
        <translation>&amp;Help</translation>
    </message>
    <message>
        <source>Der Vault ist gesperrt. Master-Passwort eingeben:</source>
        <translation type="vanished">The vault is locked. Enter the master password:</translation>
    </message>
    <message>
        <source>Noch kein Vault vorhanden. Lege ein Master-Passwort fest:</source>
        <translation type="vanished">No vault yet. Set a master password:</translation>
    </message>
    <message>
        <source>Master-Passwort</source>
        <translation type="vanished">Master password</translation>
    </message>
    <message>
        <source>Master-Passwort bestätigen</source>
        <translation type="vanished">Confirm master password</translation>
    </message>
    <message>
        <source>Entsperren</source>
        <translation type="vanished">Unlock</translation>
    </message>
    <message>
        <source>Vault anlegen</source>
        <translation type="vanished">Create vault</translation>
    </message>
    <message>
        <source>Falsches Master-Passwort.</source>
        <translation type="vanished">Wrong master password.</translation>
    </message>
    <message>
        <source>Bitte ein Passwort eingeben.</source>
        <translation type="vanished">Please enter a password.</translation>
    </message>
    <message>
        <source>Die Passwörter stimmen nicht überein.</source>
        <translation type="vanished">The passwords do not match.</translation>
    </message>
    <message>
        <source>Der Vault konnte nicht angelegt werden.</source>
        <translation type="vanished">The vault could not be created.</translation>
    </message>
    <message>
        <source>Der Vault speichert Geheimnisse (Passwörter, Passphrasen, Tokens) verschlüsselt hinter dem Master-Passwort. Das Master-Passwort wird nicht gespeichert und kann nicht wiederhergestellt werden.</source>
        <translation type="vanished">The vault stores secrets (passwords, passphrases, tokens) encrypted behind the master password. The master password is not stored and cannot be recovered.</translation>
    </message>
    <message>
        <source>Gespeicherte Geheimnisse</source>
        <translation type="vanished">Stored secrets</translation>
    </message>
    <message>
        <source>Hinzufügen</source>
        <translation type="vanished">Add</translation>
    </message>
    <message>
        <source>Verbergen</source>
        <translation type="vanished">Hide</translation>
    </message>
    <message>
        <source>Anzeigen</source>
        <translation type="vanished">Show</translation>
    </message>
    <message>
        <source>In Zwischenablage kopieren</source>
        <translation type="vanished">Copy to clipboard</translation>
    </message>
    <message>
        <source>Noch keine Geheimnisse. Mit „Hinzufügen“ eines anlegen.</source>
        <translation type="vanished">No secrets yet. Add one with “Add”.</translation>
    </message>
    <message>
        <source>Master-Passwort ändern …</source>
        <translation type="vanished">Change master password …</translation>
    </message>
    <message>
        <source>Sperren</source>
        <translation type="vanished">Lock</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4256"/>
        <source>Geheimnis</source>
        <translation>Secret</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4269"/>
        <source>z. B. ssh/prod</source>
        <translation>e.g. ssh/prod</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4270"/>
        <source>Wert</source>
        <translation>Value</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4271"/>
        <source>Passwort / Token / Passphrase</source>
        <translation>Password / token / passphrase</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4273"/>
        <source>Wert anzeigen</source>
        <translation>Show value</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4281"/>
        <source>Master-Passwort ändern</source>
        <translation>Change master password</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4288"/>
        <source>Aktuelles Master-Passwort</source>
        <translation>Current master password</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4289"/>
        <source>Neues Master-Passwort</source>
        <translation>New master password</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4290"/>
        <source>Neues Passwort bestätigen</source>
        <translation>Confirm new password</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4293"/>
        <source>Ändern</source>
        <translation>Change</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4296"/>
        <source>Bitte ein neues Passwort eingeben.</source>
        <translation>Please enter a new password.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4297"/>
        <source>Die neuen Passwörter stimmen nicht überein.</source>
        <translation>The new passwords do not match.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4298"/>
        <source>Das aktuelle Master-Passwort ist falsch.</source>
        <translation>The current master password is wrong.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4317"/>
        <source>QTmux — plattformübergreifender Multi-KI-Agenten-Terminal.
Version %1</source>
        <translation>QTmux — cross-platform multi-AI-agent terminal.
Version %1</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4337"/>
        <source>Farbschema importieren</source>
        <translation>Import color scheme</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4338"/>
        <source>Farbschemata (*.itermcolors *.Xresources *.conf *.txt)</source>
        <translation>Color schemes (*.itermcolors *.Xresources *.conf *.txt)</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4339"/>
        <source>Alle Dateien (*)</source>
        <translation>All files (*)</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4348"/>
        <source>Import fehlgeschlagen</source>
        <translation>Import failed</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4354"/>
        <source>Die Datei konnte nicht als Farbschema gelesen werden (unterstützt: iTerm .itermcolors, Xresources, Ghostty).</source>
        <translation>The file could not be read as a color scheme (supported: iTerm .itermcolors, Xresources, Ghostty).</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4363"/>
        <source>Mehrzeilig einfügen?</source>
        <translation>Paste multiple lines?</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4371"/>
        <source>Der Inhalt der Zwischenablage hat %1 Zeilen und könnte mehrere Befehle ausführen. Trotzdem einfügen?</source>
        <translation>The clipboard has %1 lines and may run multiple commands. Paste anyway?</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2635"/>
        <location filename="../qml/Main.qml" line="2638"/>
        <location filename="../qml/Main.qml" line="2641"/>
        <location filename="../qml/Main.qml" line="2644"/>
        <location filename="../qml/Main.qml" line="2938"/>
        <location filename="../qml/Main.qml" line="4415"/>
        <location filename="../qml/Main.qml" line="4628"/>
        <source>Gruppe</source>
        <translation>Group</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3095"/>
        <source>Seitenleiste einklappen (%1)</source>
        <translation>Collapse sidebar (%1)</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3262"/>
        <location filename="../qml/Main.qml" line="3641"/>
        <source>%1 Panes</source>
        <translation>%1 panes</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3647"/>
        <source>Gruppe: %1</source>
        <translation>Group: %1</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2955"/>
        <location filename="../qml/Main.qml" line="4454"/>
        <location filename="../qml/Main.qml" line="4650"/>
        <source>Neue Gruppe …</source>
        <translation>New group …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2569"/>
        <source>Beim Start automatisch nach Updates suchen</source>
        <translation>Check for updates automatically at startup</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2816"/>
        <source>MCP-Server: an (%1:%2)</source>
        <translation>MCP server: on (%1:%2)</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2853"/>
        <source>Neu</source>
        <translation>New</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2897"/>
        <source>Teilen</source>
        <translation>Split</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2913"/>
        <source>Zoom</source>
        <translation>Zoom</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2929"/>
        <source>&amp;Session</source>
        <translation>&amp;Session</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2960"/>
        <location filename="../qml/Main.qml" line="4462"/>
        <location filename="../qml/Main.qml" line="4655"/>
        <source>Aus Gruppe entfernen</source>
        <translation>Remove from group</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2993"/>
        <source>&amp;Fenster</source>
        <translation>&amp;Window</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3094"/>
        <source>Seitenleiste ausklappen (%1)</source>
        <translation>Expand sidebar (%1)</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3267"/>
        <source>Commit: %1</source>
        <translation>Commit: %1</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3267"/>
        <source>Branch: %1</source>
        <translation>Branch: %1</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4472"/>
        <location filename="../qml/Main.qml" line="4670"/>
        <source>Controller-Markierung entfernen</source>
        <translation>Remove Controller Marker</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4485"/>
        <source>Gruppe nach oben</source>
        <translation>Move Group Up</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4489"/>
        <source>Gruppe nach unten</source>
        <translation>Move Group Down</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4494"/>
        <source>Gruppe umbenennen …</source>
        <translation>Rename group …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4498"/>
        <source>Gruppe auflösen</source>
        <translation>Dissolve group</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4512"/>
        <source>In die Warteschlange einreihen</source>
        <translation>Add to queue</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4531"/>
        <source>Der Text wird abgeschickt, sobald die Session frei ist. Arbeitet dort gerade ein Agent, wartet er — so landet er nicht mitten in dessen Ausgabe.</source>
        <translation>The text is sent as soon as the session is free. If an agent is working there, it waits — so it does not end up in the middle of that agent&apos;s output.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4536"/>
        <source>z. B. Danach bitte die Tests laufen lassen</source>
        <translation>e.g. Afterwards please run the tests</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4547"/>
        <source>Gruppe umbenennen</source>
        <translation>Rename group</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4547"/>
        <source>Neue Gruppe</source>
        <translation>New group</translation>
    </message>
    <message>
        <source>Sitzungen einer Gruppe stehen in der Seitenleiste zusammen und lassen sich gemeinsam ein- und ausklappen.</source>
        <translation type="vanished">Sessions in a group are listed together in the sidebar and can be collapsed and expanded as one.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4572"/>
        <source>z. B. Release 1.5</source>
        <translation>e.g. Release 1.5</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4723"/>
        <source>QTmux beenden?</source>
        <translation>Quit QTmux?</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4763"/>
        <source>… und %1 weitere</source>
        <translation>… and %1 more</translation>
    </message>
    <message>
        <source>Rendering</source>
        <translation type="vanished">Rendering</translation>
    </message>
    <message>
        <source>GPU-Glyph-Atlas (schneller; aus = QPainter-Fallback)</source>
        <translation type="vanished">GPU glyph atlas (faster; off = QPainter fallback)</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2537"/>
        <source>Vor dem Beenden nachfragen</source>
        <translation>Ask before quitting</translation>
    </message>
    <message>
        <source>Beenden schließt alle Sitzungen samt laufender Prozesse.</source>
        <translation type="vanished">Quitting closes all sessions along with their running processes.</translation>
    </message>
    <message>
        <source>Agenten-Steuerung (MCP)</source>
        <translation type="vanished">Agent control (MCP)</translation>
    </message>
    <message>
        <source>MCP-Server aktiv (nur 127.0.0.1)</source>
        <translation type="vanished">MCP server enabled (127.0.0.1 only)</translation>
    </message>
    <message>
        <source>Übernehmen</source>
        <translation type="vanished">Apply</translation>
    </message>
    <message>
        <source>Wird gespeichert und beim nächsten Start verwendet. Die Umgebungsvariable QTMUX_MCP_PORT hat Vorrang.</source>
        <translation type="vanished">Saved and used on the next start. The environment variable QTMUX_MCP_PORT takes precedence.</translation>
    </message>
    <message>
        <source>Agenten-Benachrichtigungen</source>
        <translation type="vanished">Agent notifications</translation>
    </message>
    <message>
        <source>Wähle je Session, ob sie über Ereignisse der anderen Sessions benachrichtigt wird. Feinere Quell-Filter sind über die MCP-Schnittstelle verfügbar.</source>
        <translation type="vanished">Choose for each session whether it is notified about events from the other sessions. Finer source filters are available via the MCP interface.</translation>
    </message>
    <message>
        <source>#%1 · %2</source>
        <translation type="vanished">#%1 · %2</translation>
    </message>
    <message>
        <source>#%1</source>
        <translation type="vanished">#%1</translation>
    </message>
    <message>
        <source>Arten:</source>
        <translation type="vanished">Kinds:</translation>
    </message>
    <message>
        <source>fertig</source>
        <translation type="vanished">done</translation>
    </message>
    <message>
        <source>Frage</source>
        <translation type="vanished">question</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="961"/>
        <source>Fehler</source>
        <translation>error</translation>
    </message>
    <message>
        <source>Keine Sessions geöffnet.</source>
        <translation type="vanished">No sessions open.</translation>
    </message>
    <message>
        <source>Tastenkürzel</source>
        <translation type="vanished">Keyboard shortcuts</translation>
    </message>
    <message>
        <source>(keins)</source>
        <translation type="vanished">(none)</translation>
    </message>
    <message>
        <source>Standard</source>
        <translation type="vanished">Default</translation>
    </message>
    <message>
        <source>Alle Kürzel zurücksetzen</source>
        <translation type="vanished">Reset all shortcuts</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="687"/>
        <location filename="../qml/Main.qml" line="2591"/>
        <source>Einstellungen</source>
        <translation>Settings</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="377"/>
        <location filename="../qml/Main.qml" line="2580"/>
        <location filename="../qml/Main.qml" line="2861"/>
        <source>%1 (Plugin)</source>
        <translation>%1 (Plugin)</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="675"/>
        <source>Befehlspalette</source>
        <translation>Command palette</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2002"/>
        <location filename="../qml/Main.qml" line="2349"/>
        <location filename="../qml/Main.qml" line="2490"/>
        <source>Secrets-Vault …</source>
        <translation>Secrets Vault …</translation>
    </message>
    <message numerus="yes">
        <source>%n Pane(n)</source>
        <translation type="vanished">
            <numerusform>%n pane</numerusform>
            <numerusform>%n panes</numerusform>
        </translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4567"/>
        <source>Fenster einer Gruppe stehen in der Seitenleiste zusammen und lassen sich gemeinsam ein- und ausklappen.</source>
        <translation>Windows in a group are listed together in the sidebar and can be collapsed and expanded as one.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2046"/>
        <location filename="../qml/Main.qml" line="4592"/>
        <source>Umbenennen …</source>
        <translation>Rename …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4597"/>
        <source>Automatischer Name</source>
        <translation>Automatic Name</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4662"/>
        <source>Fenster schließen</source>
        <translation>Close Window</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4690"/>
        <source>Fenster umbenennen</source>
        <translation>Rename Window</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4705"/>
        <source>Leer lassen = automatischer Name (Titel des aktiven Panes).</source>
        <translation>Leave empty = automatic name (title of the active pane).</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4710"/>
        <source>z. B. Build, Server, Logs</source>
        <translation>e.g. Build, Server, Logs</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4735"/>
        <source>Beim Beenden werden alle offenen Sitzungen samt ihrer laufenden Prozesse und Verbindungen geschlossen.</source>
        <translation>Quitting closes all open sessions along with their running processes and connections.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="4738"/>
        <source>Offene Sitzungen:</source>
        <translation>Open sessions:</translation>
    </message>
    <message>
        <source>Tastenkürzel ändern</source>
        <translation type="vanished">Change shortcut</translation>
    </message>
    <message>
        <source>Neues Kürzel für „%1“</source>
        <translation type="vanished">New shortcut for “%1”</translation>
    </message>
    <message>
        <source>Tasten drücken …</source>
        <translation type="vanished">Press keys …</translation>
    </message>
    <message>
        <source>Bereits belegt von: %1</source>
        <translation type="vanished">Already assigned to: %1</translation>
    </message>
    <message>
        <source>Leeren</source>
        <translation type="vanished">Clear</translation>
    </message>
    <message>
        <source>Auf Standard</source>
        <translation type="vanished">To default</translation>
    </message>
    <message>
        <source>Mehrere nacheinander gedrückte Akkorde ergeben eine Tastenfolge (max. 4). Esc bricht ab, Eingabe bestätigt.</source>
        <translation type="vanished">Several chords pressed in sequence form a key sequence (max. 4). Esc cancels, Enter confirms.</translation>
    </message>
    <message>
        <source>Bitte einen Port zwischen 1024 und 65535 angeben.</source>
        <translation type="vanished">Please enter a port between 1024 and 65535.</translation>
    </message>
    <message>
        <source>Port %1 ließ sich nicht öffnen (belegt?). Server ist aus.</source>
        <translation type="vanished">Port %1 could not be opened (already in use?). Server is off.</translation>
    </message>
    <message>
        <source>Erscheinungsbild</source>
        <translation type="vanished">Appearance</translation>
    </message>
    <message>
        <source>Design</source>
        <translation type="vanished">Theme</translation>
    </message>
    <message>
        <source>Wie System</source>
        <translation type="vanished">Follow System</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2289"/>
        <source>Hell</source>
        <translation>Light</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2289"/>
        <source>Dunkel</source>
        <translation>Dark</translation>
    </message>
    <message>
        <source>Farbschema (Dunkel)</source>
        <translation type="vanished">Color scheme (dark)</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="844"/>
        <location filename="../qml/Main.qml" line="2652"/>
        <source>Fenster</source>
        <translation>Window</translation>
    </message>
    <message>
        <source>Quake-Modus: per globalem Hotkey ein-/ausblenden</source>
        <translation type="vanished">Quake mode: toggle via global hotkey</translation>
    </message>
    <message>
        <source>Globaler Hotkey: Strg+^ (blendet QTmux überall ein/aus)</source>
        <translation type="vanished">Global hotkey: Ctrl+` (toggles QTmux from anywhere)</translation>
    </message>
    <message>
        <source>Derzeit nur unter macOS verfügbar.</source>
        <translation type="vanished">Currently available on macOS only.</translation>
    </message>
    <message>
        <source>Farbschema</source>
        <translation type="vanished">Color scheme</translation>
    </message>
    <message>
        <source>Importieren …</source>
        <translation type="vanished">Import …</translation>
    </message>
    <message>
        <source>Farbschema (Hell)</source>
        <translation type="vanished">Color scheme (light)</translation>
    </message>
    <message>
        <source>Terminal</source>
        <translation type="vanished">Terminal</translation>
    </message>
    <message>
        <source>Schriftart</source>
        <translation type="vanished">Font</translation>
    </message>
    <message>
        <source>Schriftgröße</source>
        <translation type="vanished">Font size</translation>
    </message>
    <message>
        <source>Ligaturen</source>
        <translation type="vanished">Ligatures</translation>
    </message>
    <message>
        <source>Programmier-Ligaturen (z. B. FiraCode)</source>
        <translation type="vanished">Programming ligatures (e.g. FiraCode)</translation>
    </message>
    <message>
        <source>Eingabe &amp; Zwischenablage</source>
        <translation type="vanished">Input &amp; Clipboard</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="686"/>
        <location filename="../qml/Main.qml" line="2501"/>
        <source>Design umschalten</source>
        <translation>Toggle theme</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="682"/>
        <location filename="../qml/Main.qml" line="2513"/>
        <source>MCP-Server umschalten</source>
        <translation>Toggle MCP server</translation>
    </message>
    <message>
        <source>Session %1</source>
        <translation type="vanished">Session %1</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2652"/>
        <source>Wechseln zu: %1</source>
        <translation>Switch to: %1</translation>
    </message>
    <message>
        <source>Session</source>
        <translation type="vanished">Session</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2431"/>
        <source>Befehl suchen …</source>
        <translation>Search command …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2798"/>
        <source>Keine Treffer</source>
        <translation>No matches</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1988"/>
        <location filename="../qml/Main.qml" line="2338"/>
        <location filename="../qml/Main.qml" line="2488"/>
        <source>Neue serielle Verbindung …</source>
        <translation>New Serial Connection …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3865"/>
        <source>Serielle Verbindung</source>
        <translation>Serial Connection</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3846"/>
        <location filename="../qml/Main.qml" line="3882"/>
        <location filename="../qml/Main.qml" line="3991"/>
        <location filename="../qml/Main.qml" line="4046"/>
        <source>Port</source>
        <translation>Port</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3888"/>
        <location filename="../qml/Main.qml" line="4048"/>
        <source>Baudrate</source>
        <translation>Baud Rate</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3898"/>
        <source>Keine seriellen Ports gefunden.</source>
        <translation>No serial ports found.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="596"/>
        <location filename="../qml/Main.qml" line="3976"/>
        <source>Shell</source>
        <translation>Shell</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="596"/>
        <location filename="../qml/Main.qml" line="3976"/>
        <source>SSH</source>
        <translation>SSH</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="596"/>
        <location filename="../qml/Main.qml" line="3976"/>
        <source>Seriell</source>
        <translation>Serial</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="358"/>
        <source>SSH …</source>
        <translation>SSH …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="365"/>
        <source>Seriell …</source>
        <translation>Serial …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1904"/>
        <source>Im Terminal suchen …</source>
        <translation>Find in terminal …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1981"/>
        <location filename="../qml/Main.qml" line="2333"/>
        <location filename="../qml/Main.qml" line="2487"/>
        <source>Neue SSH-Verbindung …</source>
        <translation>New SSH Connection …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1995"/>
        <location filename="../qml/Main.qml" line="2343"/>
        <location filename="../qml/Main.qml" line="2489"/>
        <source>Verbindungen verwalten …</source>
        <translation>Manage Connections …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2550"/>
        <source>Unterhaltung fortsetzen: gar nicht</source>
        <translation>Continue conversation: not at all</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2551"/>
        <source>Unterhaltung fortsetzen: jüngste im Verzeichnis</source>
        <translation>Continue conversation: most recent in directory</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2552"/>
        <source>Unterhaltung fortsetzen: Auswahl beim Start</source>
        <translation>Continue conversation: pick at startup</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2553"/>
        <source>Unterhaltung fortsetzen: gemeldete Sitzung</source>
        <translation>Continue conversation: reported session</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2564"/>
        <source>Fenster umbenennen …</source>
        <translation>Rename window …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2590"/>
        <source>Standard-Shell: %1</source>
        <translation>Default shell: %1</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2599"/>
        <source>Einstellungen: %1 …</source>
        <translation>Settings: %1 …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2613"/>
        <source>SFTP: %1</source>
        <translation>SFTP: %1</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2623"/>
        <source>Aktives Fenster</source>
        <translation>Active Window</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2624"/>
        <source>Fenster gruppieren …</source>
        <translation>Group Window …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2628"/>
        <source>Fenster zu Gruppe: %1</source>
        <translation>Window to Group: %1</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2632"/>
        <source>Fenster aus Gruppe nehmen</source>
        <translation>Remove Window from Group</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2641"/>
        <source>Gruppe nach oben: %1</source>
        <translation>Move Group Up: %1</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2644"/>
        <source>Gruppe nach unten: %1</source>
        <translation>Move Group Down: %1</translation>
    </message>
    <message>
        <source>Sessions beim Start wiederherstellen</source>
        <translation type="vanished">Restore sessions on start</translation>
    </message>
    <message>
        <source>Ohne Verlauf</source>
        <translation type="vanished">Without history</translation>
    </message>
    <message>
        <source>Alles</source>
        <translation type="vanished">Everything</translation>
    </message>
    <message>
        <source>Unterhaltung fortsetzen</source>
        <translation type="vanished">Continue conversation</translation>
    </message>
    <message>
        <source>Gar nicht</source>
        <translation type="vanished">Not at all</translation>
    </message>
    <message>
        <source>Jüngste im Verzeichnis</source>
        <translation type="vanished">Most recent in directory</translation>
    </message>
    <message>
        <source>Auswahl beim Start</source>
        <translation type="vanished">Pick at startup</translation>
    </message>
    <message>
        <source>Gemeldete Sitzung</source>
        <translation type="vanished">Reported session</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3790"/>
        <source>⟫ Eingabe geht an ALLE Sessions — Strg/Cmd+Umschalt+B zum Beenden</source>
        <translation>⟫ Input goes to ALL sessions — Ctrl/Cmd+Shift+B to stop</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3817"/>
        <source>SSH-Verbindung</source>
        <translation>SSH Connection</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3842"/>
        <location filename="../qml/Main.qml" line="3987"/>
        <source>Host</source>
        <translation>Host</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3844"/>
        <location filename="../qml/Main.qml" line="3989"/>
        <source>Benutzer</source>
        <translation>User</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3848"/>
        <location filename="../qml/Main.qml" line="3993"/>
        <source>Identity-Datei</source>
        <translation>Identity File</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3852"/>
        <location filename="../qml/Main.qml" line="4059"/>
        <source>Passwort/Schlüssel werden im Terminal abgefragt (System-ssh).</source>
        <translation>Password/key will be requested in the terminal (system ssh).</translation>
    </message>
    <message>
        <source>Agent-Steuerung</source>
        <translation type="vanished">Agent Control</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2318"/>
        <source>Neue Session: %1</source>
        <translation>New session: %1</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2325"/>
        <source>Session-Typ wählen</source>
        <translation>Select session type</translation>
    </message>
    <message>
        <source>MCP-Server: an (127.0.0.1:%1)</source>
        <translation type="vanished">MCP Server: on (127.0.0.1:%1)</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2817"/>
        <source>MCP-Server: aus</source>
        <translation>MCP Server: off</translation>
    </message>
    <message>
        <source>Datei</source>
        <translation type="vanished">File</translation>
    </message>
    <message>
        <source>Bearbeiten</source>
        <translation type="vanished">Edit</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1887"/>
        <location filename="../qml/Main.qml" line="2522"/>
        <source>Kopieren</source>
        <translation>Copy</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1895"/>
        <location filename="../qml/Main.qml" line="2523"/>
        <source>Einfügen</source>
        <translation>Paste</translation>
    </message>
    <message>
        <source>Ansicht</source>
        <translation type="vanished">View</translation>
    </message>
    <message>
        <source>Sprache</source>
        <translation type="vanished">Language</translation>
    </message>
    <message>
        <source>Agent</source>
        <translation type="vanished">Agent</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2977"/>
        <source>Neue Agent-Session …</source>
        <translation>New Agent Session …</translation>
    </message>
    <message>
        <source>Hilfe</source>
        <translation type="vanished">Help</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="688"/>
        <location filename="../qml/Main.qml" line="2028"/>
        <location filename="../qml/Main.qml" line="2570"/>
        <location filename="../qml/Main.qml" line="2827"/>
        <location filename="../qml/Main.qml" line="4309"/>
        <source>Über QTmux</source>
        <translation>About QTmux</translation>
    </message>
    <message>
        <source>Agent: %1</source>
        <translation type="vanished">Agent: %1</translation>
    </message>
    <message>
        <source>+  Neue Session</source>
        <translation type="vanished">+  New Session</translation>
    </message>
    <message>
        <source>QTmux — plattformübergreifender Multi-KI-Agenten-Terminal.
Qt %1</source>
        <translation type="vanished">QTmux — cross-platform multi-AI-agent terminal.
Qt %1</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="653"/>
        <source>Standard-Shell</source>
        <translation>Default Shell</translation>
    </message>
</context>
<context>
    <name>McpAccess</name>
    <message>
        <location filename="../src/server/McpAccess.cpp" line="24"/>
        <source>Ungültige Bind-Adresse „%1“. Erlaubt sind IP-Literale (z. B. 127.0.0.1, 0.0.0.0, 192.168.0.10) sowie localhost/any. Es bleibt bei 127.0.0.1.</source>
        <translation>Invalid bind address “%1”. Allowed are IP literals (e.g. 127.0.0.1, 0.0.0.0, 192.168.0.10) as well as localhost/any. Falling back to 127.0.0.1.</translation>
    </message>
    <message>
        <location filename="../src/server/McpAccess.cpp" line="108"/>
        <source>MCP-Server nicht gestartet: Bindung an %1 macht ihn über das Netz erreichbar, es ist aber kein Zugriffs-Token gesetzt. Über MCP lässt sich beliebiger Text in laufende Terminals schreiben — ein Token ist deshalb Pflicht. Setzen in den Einstellungen (Agenten → MCP-Server) oder per QTMUX_MCP_TOKEN.</source>
        <translation>MCP server not started: binding to %1 makes it reachable over the network, but no access token is set. MCP can type arbitrary text into running terminals — a token is therefore mandatory. Set one in Settings (Agents → MCP server) or via QTMUX_MCP_TOKEN.</translation>
    </message>
</context>
<context>
    <name>PrefsWindow</name>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="22"/>
        <location filename="../qml/PrefsWindow.qml" line="288"/>
        <source>Einstellungen</source>
        <translation>Settings</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="107"/>
        <source>Allgemein</source>
        <translation>General</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="108"/>
        <source>Erscheinungsbild</source>
        <translation>Appearance</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="109"/>
        <source>Darstellung &amp; Shell</source>
        <translation>Appearance &amp; shell</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="110"/>
        <source>Eingabe &amp; Zwischenablage</source>
        <translation>Input &amp; Clipboard</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="125"/>
        <source>Terminal</source>
        <translation>Terminal</translation>
    </message>
    <message>
        <source>Eingabe</source>
        <translation type="vanished">Input</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="111"/>
        <source>Agenten &amp; MCP</source>
        <translation>Agents &amp; MCP</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="112"/>
        <source>Tastenkürzel</source>
        <translation>Keyboard shortcuts</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="113"/>
        <source>Verbindungen</source>
        <translation>Connections</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="114"/>
        <location filename="../qml/PrefsWindow.qml" line="226"/>
        <source>Secrets-Vault</source>
        <translation>Secrets Vault</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="115"/>
        <location filename="../qml/PrefsWindow.qml" line="227"/>
        <source>Erweiterungen</source>
        <translation>Extensions</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="124"/>
        <source>Arbeitsplatz</source>
        <translation>Workspace</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="126"/>
        <source>Agenten &amp; Geräte</source>
        <translation>Agents &amp; devices</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="160"/>
        <source>%1 Abos</source>
        <translation>%1 subscriptions</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="167"/>
        <source>%1 geändert</source>
        <translation>%1 changed</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="174"/>
        <source>gesperrt</source>
        <translation>locked</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="203"/>
        <source>Design</source>
        <translation>Theme</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="204"/>
        <source>Sprache</source>
        <translation>Language</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="205"/>
        <source>Vor dem Beenden nachfragen</source>
        <translation>Ask before quitting</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="206"/>
        <source>Sessions beim Start wiederherstellen</source>
        <translation>Restore sessions on start</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="207"/>
        <source>Quake-Modus</source>
        <translation>Quake mode</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="208"/>
        <source>Ruhezustand verhindern</source>
        <translation>Prevent sleep</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="209"/>
        <source>Automatisch nach Updates suchen</source>
        <translation>Check for updates automatically</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="210"/>
        <source>Farbschema (Dunkel)</source>
        <translation>Color scheme (dark)</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="211"/>
        <source>Farbschema (Hell)</source>
        <translation>Color scheme (light)</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="212"/>
        <source>Schriftart</source>
        <translation>Font</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="213"/>
        <source>Schriftgröße</source>
        <translation>Font size</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="214"/>
        <source>Ligaturen</source>
        <translation>Ligatures</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="215"/>
        <source>GPU-Glyph-Atlas</source>
        <translation>GPU glyph atlas</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="216"/>
        <source>Standard-Shell</source>
        <translation>Default Shell</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="217"/>
        <source>Auswahl automatisch kopieren</source>
        <translation>Copy on select</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="218"/>
        <source>Rechtsklick fügt ein</source>
        <translation>Right-click pastes</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="219"/>
        <source>Vor mehrzeiligem Einfügen warnen</source>
        <translation>Warn before multiline paste</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="220"/>
        <source>Mausrad in Vollbild-Anwendungen</source>
        <translation>Mouse wheel in full-screen applications</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="221"/>
        <source>Agenten beim Start wiederherstellen</source>
        <translation>Restore agents on start</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="222"/>
        <source>Unterhaltung fortsetzen</source>
        <translation>Continue conversation</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="223"/>
        <source>Benachrichtigungen</source>
        <translation>Notifications</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="224"/>
        <source>MCP-Server</source>
        <translation>MCP server</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="225"/>
        <source>Verbindungsprofile</source>
        <translation>Connection profiles</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="311"/>
        <source>Suchen — z. B. „Ligaturen“</source>
        <translation>Search — e.g. “Ligatures”</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="392"/>
        <source>Zurücksetzen</source>
        <translation>Reset</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="399"/>
        <source>Diese Seite zurücksetzen</source>
        <translation>Reset this page</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="407"/>
        <source>Alle Einstellungen zurücksetzen …</source>
        <translation>Reset all settings …</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="414"/>
        <source>Import / Export</source>
        <translation>Import / export</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="421"/>
        <source>Exportieren …</source>
        <translation>Export …</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="425"/>
        <source>Importieren …</source>
        <translation>Import …</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="575"/>
        <source>Alles wirkt sofort.</source>
        <translation>Everything applies instantly.</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="640"/>
        <source>%1 zurückgesetzt: %2 Einstellungen</source>
        <translation>%1 reset: %2 settings</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="641"/>
        <source>Nichts zurückzusetzen — diese Seite steht auf den Standardwerten.</source>
        <translation>Nothing to reset — this page is at its default values.</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="651"/>
        <source>Alle Einstellungen zurücksetzen?</source>
        <translation>Reset all settings?</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="656"/>
        <source>%1 Einstellungen zurückgesetzt.</source>
        <translation>%1 settings reset.</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="659"/>
        <source>Alle Einstellungen gehen auf die Werkseinstellung zurück — Design, Sprache, Terminal, Kürzel, Farbschemata und Verbindungsprofile.

Nicht angetastet werden: die offenen Fenster und Sessions sowie der Secrets-Vault (der liegt verschlüsselt außerhalb der Einstellungen).</source>
        <translation>All settings return to their factory values — appearance, language, terminal, shortcuts, colour schemes and connection profiles.

Left untouched: the open windows and sessions, and the secrets vault (which is stored encrypted outside the settings).</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="674"/>
        <location filename="../qml/PrefsWindow.qml" line="763"/>
        <source>Einstellungen importieren</source>
        <translation>Import settings</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="680"/>
        <source>%1 Einstellungen übernommen.</source>
        <translation>%1 settings applied.</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="681"/>
        <source>Keine Einstellung geändert.</source>
        <translation>No setting changed.</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="687"/>
        <source>Diese Schlüssel würden geändert (%1):</source>
        <translation>These keys would change (%1):</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="717"/>
        <source>wird übersprungen (unbekannter Schlüssel)</source>
        <translation>skipped (unknown key)</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="718"/>
        <source>%1 → %2</source>
        <translation>%1 → %2</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="719"/>
        <source>(nicht gesetzt)</source>
        <translation>(not set)</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="733"/>
        <source>Fehler</source>
        <translation>error</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="746"/>
        <source>Einstellungen exportieren</source>
        <translation>Export settings</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="753"/>
        <location filename="../qml/PrefsWindow.qml" line="765"/>
        <source>QTmux-Einstellungen (*.json)</source>
        <translation>QTmux settings (*.json)</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="753"/>
        <location filename="../qml/PrefsWindow.qml" line="765"/>
        <source>Alle Dateien (*)</source>
        <translation>All files (*)</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="756"/>
        <source>Exportiert nach %1</source>
        <translation>Exported to %1</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="771"/>
        <source>Die Datei enthält keine abweichenden Einstellungen.</source>
        <translation>The file contains no differing settings.</translation>
    </message>
</context>
<context>
    <name>Shells</name>
    <message>
        <location filename="../src/core/Session.cpp" line="33"/>
        <location filename="../src/core/ShellRegistry.cpp" line="105"/>
        <source>Eingabeaufforderung (Clink)</source>
        <translation>Command Prompt (Clink)</translation>
    </message>
    <message>
        <location filename="../src/core/Session.cpp" line="55"/>
        <location filename="../src/core/ShellRegistry.cpp" line="88"/>
        <source>Eingabeaufforderung</source>
        <translation>Command Prompt</translation>
    </message>
</context>
<context>
    <name>SplitNode</name>
    <message>
        <location filename="../qml/SplitNode.qml" line="312"/>
        <source>Scrollback durchsuchen …</source>
        <translation>Search scrollback …</translation>
    </message>
    <message>
        <location filename="../qml/SplitNode.qml" line="330"/>
        <source>0</source>
        <translation>0</translation>
    </message>
    <message>
        <location filename="../qml/SplitNode.qml" line="378"/>
        <source>-Klick zum Öffnen: </source>
        <translation>-click to open: </translation>
    </message>
</context>
<context>
    <name>UpdateDialog</name>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="37"/>
        <source>Nach Updates suchen …</source>
        <translation>Check for Updates …</translation>
    </message>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="38"/>
        <source>Kein Update verfügbar</source>
        <translation>No update available</translation>
    </message>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="39"/>
        <source>Update wird geladen</source>
        <translation>Downloading update</translation>
    </message>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="40"/>
        <source>Update bereit zur Installation</source>
        <translation>Update ready to install</translation>
    </message>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="41"/>
        <source>Update fehlgeschlagen</source>
        <translation>Update failed</translation>
    </message>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="42"/>
        <source>Update verfügbar</source>
        <translation>Update available</translation>
    </message>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="51"/>
        <source>%1 MB</source>
        <translation>%1 MB</translation>
    </message>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="52"/>
        <source>%1 kB</source>
        <translation>%1 kB</translation>
    </message>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="53"/>
        <source>%1 Bytes</source>
        <translation>%1 bytes</translation>
    </message>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="68"/>
        <source>QTmux fragt den Update-Server …</source>
        <translation>QTmux is contacting the update server …</translation>
    </message>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="70"/>
        <source>QTmux %1 ist aktuell.</source>
        <translation>QTmux %1 is up to date.</translation>
    </message>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="72"/>
        <source>QTmux %1</source>
        <translation>QTmux %1</translation>
    </message>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="73"/>
        <source>QTmux %1 ist verfügbar — installiert ist %2.</source>
        <translation>QTmux %1 is available — you have %2.</translation>
    </message>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="87"/>
        <source>Veröffentlicht am %1 · %2</source>
        <translation>Published %1 · %2</translation>
    </message>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="89"/>
        <source>Veröffentlicht am %1</source>
        <translation>Published %1</translation>
    </message>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="109"/>
        <source>Achtung: Das ist eine ÄLTERE Version als die installierte. Eine Rückstufung kann Einstellungen und gespeicherte Sitzungen betreffen, die eine neuere Fassung geschrieben hat.</source>
        <translation>Careful: this is an OLDER version than the one installed. Downgrading can affect settings and saved sessions that a newer release has written.</translation>
    </message>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="122"/>
        <source>Für dieses Betriebssystem liegt in dieser Veröffentlichung kein Paket bereit.</source>
        <translation>This release carries no package for your operating system.</translation>
    </message>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="128"/>
        <source>Was ist neu</source>
        <translation>What&apos;s new</translation>
    </message>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="161"/>
        <source>Heruntergeladen und geprüft: %1</source>
        <translation>Downloaded and verified: %1</translation>
    </message>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="162"/>
        <source>%1 %</source>
        <translation>%1 %</translation>
    </message>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="176"/>
        <source>QTmux läuft nicht aus einem AppImage und kann sich deshalb nicht selbst ersetzen. Die Datei oben ist geprüft und kann von Hand installiert werden.</source>
        <translation>QTmux is not running from an AppImage and therefore cannot replace itself. The file above has been verified and can be installed manually.</translation>
    </message>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="199"/>
        <source>Hinweis: Das Paket ist nicht signiert. Windows SmartScreen meldet beim Start „Der Computer wurde geschützt“ — über „Weitere Informationen“ → „Trotzdem ausführen“ fortfahren. QTmux prüft den Download selbst über eine Ed25519-Signatur und eine SHA-256-Summe.</source>
        <translation>Note: the package is not signed. Windows SmartScreen will report “Windows protected your PC” — continue via “More info” → “Run anyway”. QTmux verifies the download itself with an Ed25519 signature and a SHA-256 checksum.</translation>
    </message>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="204"/>
        <source>Hinweis: Das Paket ist nicht notariell beglaubigt. macOS Gatekeeper verweigert den ersten Start — die App im Finder mit Rechtsklick → „Öffnen“ starten. QTmux prüft den Download selbst über eine Ed25519-Signatur und eine SHA-256-Summe.</source>
        <translation>Note: the package is not notarised. macOS Gatekeeper will refuse the first launch — right-click the app in Finder and choose “Open”. QTmux verifies the download itself with an Ed25519 signature and a SHA-256 checksum.</translation>
    </message>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="208"/>
        <source>Hinweis: Das Paket ist nicht signiert. QTmux prüft den Download selbst über eine Ed25519-Signatur und eine SHA-256-Summe.</source>
        <translation>Note: the package is not signed. QTmux verifies the download itself with an Ed25519 signature and a SHA-256 checksum.</translation>
    </message>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="219"/>
        <source>Herunterladen</source>
        <translation>Download</translation>
    </message>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="225"/>
        <source>Installieren …</source>
        <translation>Install …</translation>
    </message>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="239"/>
        <source>Erneut versuchen</source>
        <translation>Try again</translation>
    </message>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="246"/>
        <source>Abbrechen</source>
        <translation>Cancel</translation>
    </message>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="252"/>
        <source>Version überspringen</source>
        <translation>Skip this version</translation>
    </message>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="259"/>
        <source>Später</source>
        <translation>Later</translation>
    </message>
    <message>
        <location filename="../qml/dialogs/UpdateDialog.qml" line="259"/>
        <source>Schließen</source>
        <translation>Close</translation>
    </message>
</context>
<context>
    <name>WindowModel</name>
    <message>
        <location filename="../src/viewmodels/WindowModel.cpp" line="49"/>
        <source>Fenster %1</source>
        <translation>Window %1</translation>
    </message>
</context>
<context>
    <name>qtmux::McpServer</name>
    <message>
        <location filename="../src/server/McpServer.cpp" line="216"/>
        <source>Port %1 auf %2 ließ sich nicht öffnen: %3</source>
        <translation>Could not open port %1 on %2: %3</translation>
    </message>
    <message>
        <location filename="../src/server/McpServer.cpp" line="221"/>
        <source>Erreichbar auf %1:%2 — Anfragen brauchen einen Authorization: Bearer &lt;token&gt;-Kopf.</source>
        <translation>Reachable at %1:%2 — requests need an Authorization: Bearer &lt;token&gt; header.</translation>
    </message>
</context>
<context>
    <name>qtmux::SettingsIo</name>
    <message>
        <location filename="../src/viewmodels/SettingsIo.cpp" line="206"/>
        <source>Kein Zielpfad angegeben.</source>
        <translation>No target path given.</translation>
    </message>
    <message>
        <location filename="../src/viewmodels/SettingsIo.cpp" line="213"/>
        <location filename="../src/viewmodels/SettingsIo.cpp" line="218"/>
        <source>Datei kann nicht geschrieben werden: %1</source>
        <translation>Cannot write file: %1</translation>
    </message>
    <message>
        <location filename="../src/viewmodels/SettingsIo.cpp" line="230"/>
        <source>Datei kann nicht gelesen werden: %1</source>
        <translation>Cannot read file: %1</translation>
    </message>
    <message>
        <location filename="../src/viewmodels/SettingsIo.cpp" line="236"/>
        <source>Keine gültige JSON-Datei: %1</source>
        <translation>Not a valid JSON file: %1</translation>
    </message>
    <message>
        <location filename="../src/viewmodels/SettingsIo.cpp" line="241"/>
        <source>Keine QTmux-Einstellungsdatei.</source>
        <translation>Not a QTmux settings file.</translation>
    </message>
</context>
<context>
    <name>qtmux::SftpClient</name>
    <message>
        <location filename="../src/viewmodels/SftpClient.cpp" line="37"/>
        <source>Kein Host angegeben.</source>
        <translation>No host given.</translation>
    </message>
    <message>
        <location filename="../src/viewmodels/SftpClient.cpp" line="58"/>
        <source>Verbinde mit %1 …</source>
        <translation>Connecting to %1 …</translation>
    </message>
    <message>
        <location filename="../src/viewmodels/SftpClient.cpp" line="61"/>
        <source>sftp konnte nicht gestartet werden.</source>
        <translation>Could not start sftp.</translation>
    </message>
    <message>
        <location filename="../src/viewmodels/SftpClient.cpp" line="68"/>
        <source>Zeitüberschreitung beim Verbindungsaufbau.</source>
        <translation>Connection timed out.</translation>
    </message>
    <message>
        <location filename="../src/viewmodels/SftpClient.cpp" line="111"/>
        <source>Verbunden.</source>
        <translation>Connected.</translation>
    </message>
    <message numerus="yes">
        <location filename="../src/viewmodels/SftpClient.cpp" line="131"/>
        <source>%n Einträge</source>
        <translation>
            <numerusform>%n entry</numerusform>
            <numerusform>%n entries</numerusform>
        </translation>
    </message>
    <message>
        <location filename="../src/viewmodels/SftpClient.cpp" line="135"/>
        <source>Verzeichniswechsel fehlgeschlagen: %1</source>
        <translation>Could not change directory: %1</translation>
    </message>
    <message>
        <location filename="../src/viewmodels/SftpClient.cpp" line="144"/>
        <source>Download fehlgeschlagen.</source>
        <translation>Download failed.</translation>
    </message>
    <message>
        <location filename="../src/viewmodels/SftpClient.cpp" line="145"/>
        <location filename="../src/viewmodels/SftpClient.cpp" line="146"/>
        <source>Heruntergeladen: %1</source>
        <translation>Downloaded: %1</translation>
    </message>
    <message>
        <location filename="../src/viewmodels/SftpClient.cpp" line="149"/>
        <source>Upload fehlgeschlagen.</source>
        <translation>Upload failed.</translation>
    </message>
    <message>
        <location filename="../src/viewmodels/SftpClient.cpp" line="150"/>
        <source>Hochgeladen: %1</source>
        <translation>Uploaded: %1</translation>
    </message>
    <message>
        <location filename="../src/viewmodels/SftpClient.cpp" line="172"/>
        <source>Verbindung fehlgeschlagen.</source>
        <translation>Connection failed.</translation>
    </message>
    <message>
        <location filename="../src/viewmodels/SftpClient.cpp" line="174"/>
        <source>Verbindung geschlossen.</source>
        <translation>Connection closed.</translation>
    </message>
    <message>
        <location filename="../src/viewmodels/SftpClient.cpp" line="208"/>
        <source>Lade herunter: %1 …</source>
        <translation>Downloading: %1 …</translation>
    </message>
    <message>
        <location filename="../src/viewmodels/SftpClient.cpp" line="220"/>
        <source>Lade hoch: %1 …</source>
        <translation>Uploading: %1 …</translation>
    </message>
</context>
<context>
    <name>qtmux::UpdateViewModel</name>
    <message>
        <location filename="../src/viewmodels/UpdateViewModel.cpp" line="237"/>
        <source>Für dieses Betriebssystem liegt kein Paket bereit.</source>
        <translation>No package is available for this operating system.</translation>
    </message>
    <message>
        <location filename="../src/viewmodels/UpdateViewModel.cpp" line="273"/>
        <source>QTmux läuft nicht aus einem AppImage und kann sich deshalb nicht selbst ersetzen. Die geprüfte Datei liegt hier — bitte von Hand installieren:
%1</source>
        <translation>QTmux is not running from an AppImage and therefore cannot replace itself. The verified file is here — please install it manually:
%1</translation>
    </message>
</context>
</TS>

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
        <location filename="../qml/prefs/CatAgenten.qml" line="96"/>
        <source>Wiederherstellung</source>
        <translation>Restore</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="98"/>
        <source>Agenten beim Start wiederherstellen</source>
        <translation>Restore agents on start</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="103"/>
        <source>Setzt in jedem Pane den zuletzt erkannten Agenten erneut ab, sobald die Shell bereit ist. Es wird ausschließlich ein bekannter Agent gestartet — beliebige Befehle laufen nicht automatisch los.</source>
        <translation>Re-runs the last detected agent in every pane as soon as the shell is ready. Only a known agent is started — arbitrary commands are never run automatically.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="119"/>
        <source>Unterhaltung fortsetzen</source>
        <translation>Continue conversation</translation>
    </message>
    <message>
        <source>Hängt das Fortsetzungs-Argument des Agenten an (z. B. --continue), sodass er die vorherige Unterhaltung weiterführt. Nur bei Agenten, die das können; ohne vorherige Unterhaltung meldet der Agent einen Fehler.</source>
        <translation type="vanished">Appends the agent&apos;s continuation argument (e.g. --continue) so it picks up the previous conversation. Only for agents that support it; without a previous conversation the agent reports an error.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="122"/>
        <source>Gar nicht</source>
        <translation>Not at all</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="122"/>
        <source>Jüngste im Verzeichnis</source>
        <translation>Most recent in directory</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="123"/>
        <source>Auswahl beim Start</source>
        <translation>Pick at startup</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="123"/>
        <source>Gemeldete Sitzung</source>
        <translation>Reported session</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="133"/>
        <source>Der Agent nimmt die JÜNGSTE Unterhaltung seines Arbeitsverzeichnisses. Richtig, solange dort nur ein Agent arbeitet — laufen mehrere im selben Ordner, bekommen sie alle dieselbe.</source>
        <translation>The agent picks the MOST RECENT conversation in its working directory. Correct as long as only one agent works there — if several run in the same folder, they all get the same one.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="136"/>
        <source>Der Agent öffnet beim Start seine eigene Auswahlliste; du entscheidest je Pane. Es wird nichts geraten, kostet aber einen Klick. Derzeit bietet nur Claude Code eine solche Liste an.</source>
        <translation>The agent opens its own picker at startup; you decide per pane. Nothing is guessed, but it costs a click. Currently only Claude Code offers such a list.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="139"/>
        <source>Genau die Unterhaltung, die der Agent zuletzt selbst gemeldet hat (MCP-Werkzeug set_agent_session) — auch bei mehreren Agenten im selben Ordner eindeutig. Meldet er nichts, startet er frisch. QTmux kann die Kennung nicht selbst ermitteln: sie entsteht im Agenten und ändert sich bei /resume oder /clear.</source>
        <translation>Exactly the conversation the agent last reported itself (MCP tool set_agent_session) — unambiguous even with several agents in the same folder. If it reports nothing, it starts fresh. QTmux cannot determine the identifier itself: it originates inside the agent and changes on /resume or /clear.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="144"/>
        <source>Der Agent startet mit einer frischen Unterhaltung.</source>
        <translation>The agent starts with a fresh conversation.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="162"/>
        <source>Benachrichtigungen</source>
        <translation>Notifications</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="164"/>
        <source>Zeile = Empfänger, Spalte = Quelle. Ein Häkchen bedeutet: der Empfänger wird über Ereignisse der Quelle benachrichtigt. Agenten abonnieren sich meist selbst per MCP (subscribe_events).</source>
        <translation>Row = receiver, column = source. A check means the receiver is notified about the source&apos;s events. Agents usually subscribe themselves via MCP (subscribe_events).</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="214"/>
        <source>Arten</source>
        <translation>Types</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="321"/>
        <source>Keine Sessions geöffnet.</source>
        <translation>No sessions open.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="333"/>
        <source>Agenten-Steuerung (MCP)</source>
        <translation>Agent control (MCP)</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="335"/>
        <source>MCP-Server aktiv (nur 127.0.0.1)</source>
        <translation>MCP server enabled (127.0.0.1 only)</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="343"/>
        <source>Port</source>
        <translation>Port</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="353"/>
        <source>Übernehmen</source>
        <translation>Apply</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="362"/>
        <source>Nur 127.0.0.1 · Vault nicht über MCP erreichbar. Wird gespeichert und beim nächsten Start verwendet; QTMUX_MCP_PORT hat Vorrang.</source>
        <translation>127.0.0.1 only · Vault not reachable via MCP. Saved and used on next start; QTMUX_MCP_PORT takes precedence.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="377"/>
        <source>Bitte einen Port zwischen 1024 und 65535 angeben.</source>
        <translation>Please enter a port between 1024 and 65535.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAgenten.qml" line="383"/>
        <source>Port %1 ließ sich nicht öffnen (belegt?). Server ist aus.</source>
        <translation>Port %1 could not be opened (already in use?). Server is off.</translation>
    </message>
</context>
<context>
    <name>CatAllgemein</name>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="11"/>
        <source>Allgemein</source>
        <translation>General</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="12"/>
        <source>Sprache, Erscheinungs-Modus und Verhalten beim Beenden.</source>
        <translation>Language, appearance mode and quit behavior.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="23"/>
        <source>Design</source>
        <translation>Theme</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="26"/>
        <source>Wie System</source>
        <translation>Follow System</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="26"/>
        <source>Hell</source>
        <translation>Light</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="26"/>
        <source>Dunkel</source>
        <translation>Dark</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="30"/>
        <source>Sprache</source>
        <translation>Language</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="48"/>
        <source>Fenster</source>
        <translation>Window</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="50"/>
        <source>Vor dem Beenden nachfragen</source>
        <translation>Ask before quitting</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="55"/>
        <source>Beenden schließt alle Sitzungen samt laufender Prozesse.</source>
        <translation>Quitting closes all sessions along with their running processes.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="62"/>
        <source>Quake-Modus: per globalem Hotkey ein-/ausblenden</source>
        <translation>Quake mode: toggle via global hotkey</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="69"/>
        <source>Globaler Hotkey: Strg+^ (blendet QTmux überall ein/aus)</source>
        <translation>Global hotkey: Ctrl+` (toggles QTmux from anywhere)</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatAllgemein.qml" line="70"/>
        <source>Derzeit nur unter macOS verfügbar.</source>
        <translation>Currently available on macOS only.</translation>
    </message>
</context>
<context>
    <name>CatEingabe</name>
    <message>
        <location filename="../qml/prefs/CatEingabe.qml" line="10"/>
        <source>Eingabe</source>
        <translation>Input</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatEingabe.qml" line="11"/>
        <source>Auswahl und Zwischenablage.</source>
        <translation>Selection and clipboard.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatEingabe.qml" line="20"/>
        <source>Auswahl automatisch kopieren</source>
        <translation>Copy on select</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatEingabe.qml" line="25"/>
        <source>Rechtsklick fügt ein</source>
        <translation>Right-click pastes</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatEingabe.qml" line="30"/>
        <source>Vor mehrzeiligem Einfügen warnen</source>
        <translation>Warn before multiline paste</translation>
    </message>
</context>
<context>
    <name>CatErscheinungsbild</name>
    <message>
        <location filename="../qml/prefs/CatErscheinungsbild.qml" line="11"/>
        <source>Erscheinungsbild</source>
        <translation>Appearance</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatErscheinungsbild.qml" line="12"/>
        <source>Farbschemata für Dunkel und Hell. Das aktive Schema färbt die gesamte App.</source>
        <translation>Color schemes for dark and light. The active scheme colors the entire app.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatErscheinungsbild.qml" line="25"/>
        <source>Farbschema (Dunkel)</source>
        <translation>Color scheme (dark)</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatErscheinungsbild.qml" line="36"/>
        <source>Importieren …</source>
        <translation>Import …</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatErscheinungsbild.qml" line="57"/>
        <source>Farbschema (Hell)</source>
        <translation>Color scheme (light)</translation>
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
        <location filename="../qml/prefs/CatTerminal.qml" line="11"/>
        <source>Terminal</source>
        <translation>Terminal</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatTerminal.qml" line="12"/>
        <source>Schrift, Ligaturen und Rendering des Terminals.</source>
        <translation>Font, ligatures and rendering of the terminal.</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatTerminal.qml" line="24"/>
        <source>Schriftart</source>
        <translation>Font</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatTerminal.qml" line="31"/>
        <source>Schriftgröße</source>
        <translation>Font size</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatTerminal.qml" line="37"/>
        <source>Ligaturen</source>
        <translation>Ligatures</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatTerminal.qml" line="39"/>
        <source>Programmier-Ligaturen (z. B. FiraCode)</source>
        <translation>Programming ligatures (e.g. FiraCode)</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatTerminal.qml" line="43"/>
        <source>Rendering</source>
        <translation>Rendering</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatTerminal.qml" line="45"/>
        <source>GPU-Glyph-Atlas (schneller; aus = QPainter-Fallback)</source>
        <translation>GPU glyph atlas (faster; off = QPainter fallback)</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatTerminal.qml" line="50"/>
        <source>Standard-Shell</source>
        <translation>Default Shell</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatTerminal.qml" line="77"/>
        <source>Vorschau</source>
        <translation>Preview</translation>
    </message>
    <message>
        <location filename="../qml/prefs/CatTerminal.qml" line="107"/>
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
        <location filename="../qml/Main.qml" line="597"/>
        <location filename="../qml/Main.qml" line="1400"/>
        <location filename="../qml/Main.qml" line="1818"/>
        <source>Neue Session</source>
        <translation>New Session</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="599"/>
        <location filename="../qml/Main.qml" line="1419"/>
        <location filename="../qml/Main.qml" line="1691"/>
        <location filename="../qml/Main.qml" line="1823"/>
        <source>Session schließen</source>
        <translation>Close Session</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1426"/>
        <location filename="../qml/Main.qml" line="2037"/>
        <source>Helles Design</source>
        <translation>Light Theme</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1426"/>
        <location filename="../qml/Main.qml" line="2037"/>
        <source>Dunkles Design</source>
        <translation>Dark Theme</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="620"/>
        <location filename="../qml/Main.qml" line="1433"/>
        <location filename="../qml/Main.qml" line="1859"/>
        <source>Beenden</source>
        <translation>Quit</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1441"/>
        <location filename="../qml/Main.qml" line="1832"/>
        <location filename="../qml/Main.qml" line="2049"/>
        <source>Einstellungen …</source>
        <translation>Settings …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1452"/>
        <location filename="../qml/Main.qml" line="1827"/>
        <source>Schrift vergrößern</source>
        <translation>Increase font size</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1459"/>
        <location filename="../qml/Main.qml" line="1828"/>
        <source>Schrift verkleinern</source>
        <translation>Decrease font size</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="616"/>
        <location filename="../qml/Main.qml" line="1466"/>
        <location filename="../qml/Main.qml" line="1829"/>
        <source>Schriftgröße zurücksetzen</source>
        <translation>Reset font size</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="610"/>
        <location filename="../qml/Main.qml" line="1474"/>
        <location filename="../qml/Main.qml" line="1830"/>
        <source>Eingabe an alle Sessions</source>
        <translation>Send input to all sessions</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="606"/>
        <location filename="../qml/Main.qml" line="1510"/>
        <location filename="../qml/Main.qml" line="1700"/>
        <location filename="../qml/Main.qml" line="1824"/>
        <source>Nebeneinander teilen</source>
        <translation>Split side by side</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="607"/>
        <location filename="../qml/Main.qml" line="1517"/>
        <location filename="../qml/Main.qml" line="1705"/>
        <location filename="../qml/Main.qml" line="1825"/>
        <source>Untereinander teilen</source>
        <translation>Split top and bottom</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="600"/>
        <location filename="../qml/Main.qml" line="1524"/>
        <location filename="../qml/Main.qml" line="1826"/>
        <source>Pane schließen</source>
        <translation>Close pane</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1554"/>
        <source>Befehlspalette …</source>
        <translation>Command palette …</translation>
    </message>
    <message>
        <source>Befehlspalette (Strg/Cmd+K)</source>
        <translation type="vanished">Command palette (Ctrl/Cmd+K)</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1714"/>
        <source>Broadcast-Eingabe: an (an alle Sessions)</source>
        <translation>Broadcast input: on (to all sessions)</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1715"/>
        <source>Eingabe an alle Sessions (Broadcast)</source>
        <translation>Send input to all sessions (broadcast)</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1875"/>
        <source>Verbinden: %1</source>
        <translation>Connect: %1</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1844"/>
        <location filename="../qml/Main.qml" line="2120"/>
        <source>Auswahl automatisch kopieren</source>
        <translation>Copy on select</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1845"/>
        <location filename="../qml/Main.qml" line="2127"/>
        <source>Rechtsklick fügt ein</source>
        <translation>Right-click pastes</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1846"/>
        <location filename="../qml/Main.qml" line="2134"/>
        <source>Vor mehrzeiligem Einfügen warnen</source>
        <translation>Warn before multiline paste</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1853"/>
        <location filename="../qml/Main.qml" line="2173"/>
        <source>Design: Wie System</source>
        <translation>Theme: Follow System</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1854"/>
        <location filename="../qml/Main.qml" line="2180"/>
        <source>Design: Hell</source>
        <translation>Theme: Light</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1855"/>
        <location filename="../qml/Main.qml" line="2187"/>
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
        <location filename="../qml/Main.qml" line="2761"/>
        <source>Verbindungsprofil</source>
        <translation>Connection Profile</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2821"/>
        <location filename="../qml/Main.qml" line="3119"/>
        <source>Name</source>
        <translation>Name</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2822"/>
        <source>z. B. Prod-Server</source>
        <translation>e.g. Prod Server</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2823"/>
        <source>Typ</source>
        <translation>Type</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2846"/>
        <source>Passwort (Vault)</source>
        <translation>Password (vault)</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2856"/>
        <source>(keines)</source>
        <translation>(none)</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2871"/>
        <source>Vault gesperrt – beim Verbinden entsperren, sonst kein Auto-Fill.</source>
        <translation>Vault locked – unlock before connecting, otherwise no auto-fill.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2884"/>
        <source>Programm</source>
        <translation>Program</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2885"/>
        <source>leer = Standard-Shell</source>
        <translation>empty = default shell</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2886"/>
        <source>Arbeitsverzeichnis</source>
        <translation>Working directory</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2887"/>
        <source>leer = Home</source>
        <translation>empty = home</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2918"/>
        <source>Befehle nach Verbindung (eine pro Zeile)</source>
        <translation>Commands after connect (one per line)</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2937"/>
        <source>z. B. cd ~/projekt
source .venv/bin/activate</source>
        <translation>e.g. cd ~/project
source .venv/bin/activate</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2943"/>
        <source>Werden gesendet, sobald die Shell bereit ist (Shell-Integration: am ersten Prompt, sonst kurz nach Verbindungsaufbau). Geeignet für key-/agent-authentifizierte Verbindungen.</source>
        <translation>Sent as soon as the shell is ready (with shell integration: at the first prompt, otherwise shortly after connect). Suited to key/agent-authenticated connections.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2961"/>
        <source>Zielordner für den Download</source>
        <translation>Download destination folder</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2966"/>
        <source>Datei zum Hochladen</source>
        <translation>File to upload</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2974"/>
        <source>SFTP – %1</source>
        <translation>SFTP – %1</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2989"/>
        <source>Übergeordnetes Verzeichnis</source>
        <translation>Parent directory</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3008"/>
        <source>Aktualisieren</source>
        <translation>Refresh</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3075"/>
        <source>Herunterladen</source>
        <translation>Download</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3086"/>
        <source>Hochladen …</source>
        <translation>Upload …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="614"/>
        <source>Secrets-Vault</source>
        <translation>Secrets Vault</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="131"/>
        <location filename="../qml/Main.qml" line="185"/>
        <location filename="../qml/Main.qml" line="197"/>
        <location filename="../qml/Main.qml" line="203"/>
        <source>Unbekannte windowId.</source>
        <translation>Unknown windowId.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="137"/>
        <source>Kein Layout vorhanden.</source>
        <translation>No layout available.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="208"/>
        <source>Letztes Fenster: Schließen würde QTmux beenden — über MCP nicht möglich. Nutze close_pane/close_session oder beende die App in der Oberfläche.</source>
        <translation>Last window: closing it would quit QTmux — not possible via MCP. Use close_pane/close_session, or quit the app from the user interface.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="220"/>
        <location filename="../qml/Main.qml" line="228"/>
        <location filename="../qml/Main.qml" line="235"/>
        <source>Unbekannte paneId.</source>
        <translation>Unknown paneId.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="241"/>
        <source>assign_session entfällt im Window-Modell — nutze focus_session bzw. focus_window.</source>
        <translation>assign_session is obsolete in the window model — use focus_session or focus_window.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="245"/>
        <source>Unbekanntes Profil.</source>
        <translation>Unknown profile.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="598"/>
        <location filename="../qml/Main.qml" line="1408"/>
        <location filename="../qml/Main.qml" line="1817"/>
        <source>Neues Fenster</source>
        <translation>New Window</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="601"/>
        <location filename="../qml/Main.qml" line="1531"/>
        <location filename="../qml/Main.qml" line="1839"/>
        <source>Nächstes Pane</source>
        <translation>Next Pane</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="602"/>
        <location filename="../qml/Main.qml" line="1538"/>
        <location filename="../qml/Main.qml" line="1840"/>
        <source>Vorheriges Pane</source>
        <translation>Previous Pane</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="603"/>
        <location filename="../qml/Main.qml" line="1545"/>
        <location filename="../qml/Main.qml" line="1841"/>
        <source>Pane zoomen</source>
        <translation>Zoom Pane</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="604"/>
        <location filename="../qml/Main.qml" line="1564"/>
        <location filename="../qml/Main.qml" line="1837"/>
        <source>Nächste Session</source>
        <translation>Next session</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="605"/>
        <location filename="../qml/Main.qml" line="1571"/>
        <location filename="../qml/Main.qml" line="1838"/>
        <source>Vorige Session</source>
        <translation>Previous session</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="609"/>
        <location filename="../qml/Main.qml" line="1836"/>
        <source>Suchen (Scrollback)</source>
        <translation>Search (Scrollback)</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="611"/>
        <source>Neue SSH-Verbindung</source>
        <translation>New SSH connection</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="612"/>
        <source>Neue serielle Verbindung</source>
        <translation>New serial connection</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="613"/>
        <source>Verbindungen verwalten</source>
        <translation>Manage connections</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="769"/>
        <location filename="../qml/Main.qml" line="773"/>
        <source>Fenster %1</source>
        <translation>Window %1</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1413"/>
        <source>Kein freier MCP-Port für eine neue Instanz gefunden.</source>
        <translation>No free MCP port found for a new instance.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1414"/>
        <source>Neues Fenster gestartet (MCP-Port %1).</source>
        <translation>New window started (MCP port %1).</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1502"/>
        <source>Suchen …</source>
        <translation>Search …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1545"/>
        <location filename="../qml/Main.qml" line="1841"/>
        <source>Pane-Zoom aufheben</source>
        <translation>Unzoom Pane</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1842"/>
        <source>Ligaturen umschalten</source>
        <translation>Toggle Ligatures</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1843"/>
        <source>GPU-Rendering umschalten</source>
        <translation>Toggle GPU Rendering</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1848"/>
        <location filename="../qml/Main.qml" line="2218"/>
        <source>Agenten beim Start wiederherstellen</source>
        <translation>Restore agents on start</translation>
    </message>
    <message>
        <source>Agenten-Unterhaltung fortsetzen</source>
        <translation type="vanished">Continue agent conversation</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1856"/>
        <source>Sprache: Deutsch</source>
        <translation>Language: German</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1857"/>
        <source>Sprache: English</source>
        <translation>Language: English</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1863"/>
        <source>Quake-Modus umschalten</source>
        <translation>Toggle Quake Mode</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1868"/>
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
        <location filename="../qml/Main.qml" line="1904"/>
        <source>Gruppe umbenennen: %1 …</source>
        <translation>Rename group: %1 …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1907"/>
        <source>Gruppe auflösen: %1</source>
        <translation>Dissolve group: %1</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2063"/>
        <source>&amp;Datei</source>
        <translation>&amp;File</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2108"/>
        <source>&amp;Bearbeiten</source>
        <translation>&amp;Edit</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2142"/>
        <source>&amp;Ansicht</source>
        <translation>&amp;View</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2195"/>
        <source>&amp;Sprache</source>
        <translation>&amp;Language</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2209"/>
        <source>A&amp;gent</source>
        <translation>A&amp;gent</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2241"/>
        <source>Agent-S&amp;teuerung</source>
        <translation>Agent &amp;Control</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2252"/>
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
        <location filename="../qml/Main.qml" line="3107"/>
        <source>Geheimnis</source>
        <translation>Secret</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3120"/>
        <source>z. B. ssh/prod</source>
        <translation>e.g. ssh/prod</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3121"/>
        <source>Wert</source>
        <translation>Value</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3122"/>
        <source>Passwort / Token / Passphrase</source>
        <translation>Password / token / passphrase</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3124"/>
        <source>Wert anzeigen</source>
        <translation>Show value</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3132"/>
        <source>Master-Passwort ändern</source>
        <translation>Change master password</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3139"/>
        <source>Aktuelles Master-Passwort</source>
        <translation>Current master password</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3140"/>
        <source>Neues Master-Passwort</source>
        <translation>New master password</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3141"/>
        <source>Neues Passwort bestätigen</source>
        <translation>Confirm new password</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3144"/>
        <source>Ändern</source>
        <translation>Change</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3147"/>
        <source>Bitte ein neues Passwort eingeben.</source>
        <translation>Please enter a new password.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3148"/>
        <source>Die neuen Passwörter stimmen nicht überein.</source>
        <translation>The new passwords do not match.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3149"/>
        <source>Das aktuelle Master-Passwort ist falsch.</source>
        <translation>The current master password is wrong.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3168"/>
        <source>QTmux — plattformübergreifender Multi-KI-Agenten-Terminal.
Version %1</source>
        <translation>QTmux — cross-platform multi-AI-agent terminal.
Version %1</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3175"/>
        <source>Farbschema importieren</source>
        <translation>Import color scheme</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3176"/>
        <source>Farbschemata (*.itermcolors *.Xresources *.conf *.txt)</source>
        <translation>Color schemes (*.itermcolors *.Xresources *.conf *.txt)</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3177"/>
        <source>Alle Dateien (*)</source>
        <translation>All files (*)</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3186"/>
        <source>Import fehlgeschlagen</source>
        <translation>Import failed</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3192"/>
        <source>Die Datei konnte nicht als Farbschema gelesen werden (unterstützt: iTerm .itermcolors, Xresources, Ghostty).</source>
        <translation>The file could not be read as a color scheme (supported: iTerm .itermcolors, Xresources, Ghostty).</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3201"/>
        <source>Mehrzeilig einfügen?</source>
        <translation>Paste multiple lines?</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3209"/>
        <source>Der Inhalt der Zwischenablage hat %1 Zeilen und könnte mehrere Befehle ausführen. Trotzdem einfügen?</source>
        <translation>The clipboard has %1 lines and may run multiple commands. Paste anyway?</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1904"/>
        <location filename="../qml/Main.qml" line="1907"/>
        <location filename="../qml/Main.qml" line="1910"/>
        <location filename="../qml/Main.qml" line="1913"/>
        <location filename="../qml/Main.qml" line="3253"/>
        <location filename="../qml/Main.qml" line="3407"/>
        <source>Gruppe</source>
        <translation>Group</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3292"/>
        <location filename="../qml/Main.qml" line="3429"/>
        <source>Neue Gruppe …</source>
        <translation>New group …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3300"/>
        <location filename="../qml/Main.qml" line="3434"/>
        <source>Aus Gruppe entfernen</source>
        <translation>Remove from group</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3310"/>
        <location filename="../qml/Main.qml" line="3449"/>
        <source>Controller-Markierung entfernen</source>
        <translation>Remove Controller Marker</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3323"/>
        <source>Gruppe nach oben</source>
        <translation>Move Group Up</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3327"/>
        <source>Gruppe nach unten</source>
        <translation>Move Group Down</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3332"/>
        <source>Gruppe umbenennen …</source>
        <translation>Rename group …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3336"/>
        <source>Gruppe auflösen</source>
        <translation>Dissolve group</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3349"/>
        <source>Gruppe umbenennen</source>
        <translation>Rename group</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3349"/>
        <source>Neue Gruppe</source>
        <translation>New group</translation>
    </message>
    <message>
        <source>Sitzungen einer Gruppe stehen in der Seitenleiste zusammen und lassen sich gemeinsam ein- und ausklappen.</source>
        <translation type="vanished">Sessions in a group are listed together in the sidebar and can be collapsed and expanded as one.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3374"/>
        <source>z. B. Release 1.5</source>
        <translation>e.g. Release 1.5</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3502"/>
        <source>QTmux beenden?</source>
        <translation>Quit QTmux?</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3542"/>
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
        <location filename="../qml/Main.qml" line="1847"/>
        <location filename="../qml/Main.qml" line="2099"/>
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
        <source>Fehler</source>
        <translation type="vanished">error</translation>
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
        <location filename="../qml/Main.qml" line="618"/>
        <source>Einstellungen</source>
        <translation>Settings</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="357"/>
        <location filename="../qml/Main.qml" line="1868"/>
        <source>%1 (Plugin)</source>
        <translation>%1 (Plugin)</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="608"/>
        <source>Befehlspalette</source>
        <translation>Command palette</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1600"/>
        <location filename="../qml/Main.qml" line="1683"/>
        <location filename="../qml/Main.qml" line="1822"/>
        <source>Secrets-Vault …</source>
        <translation>Secrets Vault …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3369"/>
        <source>Fenster einer Gruppe stehen in der Seitenleiste zusammen und lassen sich gemeinsam ein- und ausklappen.</source>
        <translation>Windows in a group are listed together in the sidebar and can be collapsed and expanded as one.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3391"/>
        <source>Umbenennen …</source>
        <translation>Rename …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3396"/>
        <source>Automatischer Name</source>
        <translation>Automatic Name</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3441"/>
        <source>Fenster schließen</source>
        <translation>Close Window</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3469"/>
        <source>Fenster umbenennen</source>
        <translation>Rename Window</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3484"/>
        <source>Leer lassen = automatischer Name (Titel des aktiven Panes).</source>
        <translation>Leave empty = automatic name (title of the active pane).</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3489"/>
        <source>z. B. Build, Server, Logs</source>
        <translation>e.g. Build, Server, Logs</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3514"/>
        <source>Beim Beenden werden alle offenen Sitzungen samt ihrer laufenden Prozesse und Verbindungen geschlossen.</source>
        <translation>Quitting closes all open sessions along with their running processes and connections.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="3517"/>
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
        <source>Hell</source>
        <translation type="vanished">Light</translation>
    </message>
    <message>
        <source>Dunkel</source>
        <translation type="vanished">Dark</translation>
    </message>
    <message>
        <source>Farbschema (Dunkel)</source>
        <translation type="vanished">Color scheme (dark)</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="766"/>
        <location filename="../qml/Main.qml" line="1921"/>
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
        <location filename="../qml/Main.qml" line="617"/>
        <location filename="../qml/Main.qml" line="1831"/>
        <source>Design umschalten</source>
        <translation>Toggle theme</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="615"/>
        <location filename="../qml/Main.qml" line="1607"/>
        <location filename="../qml/Main.qml" line="1833"/>
        <source>MCP-Server umschalten</source>
        <translation>Toggle MCP server</translation>
    </message>
    <message>
        <source>Session %1</source>
        <translation type="vanished">Session %1</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1921"/>
        <source>Wechseln zu: %1</source>
        <translation>Switch to: %1</translation>
    </message>
    <message>
        <source>Session</source>
        <translation type="vanished">Session</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1765"/>
        <source>Befehl suchen …</source>
        <translation>Search command …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2025"/>
        <source>Keine Treffer</source>
        <translation>No matches</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1586"/>
        <location filename="../qml/Main.qml" line="1672"/>
        <location filename="../qml/Main.qml" line="1820"/>
        <source>Neue serielle Verbindung …</source>
        <translation>New Serial Connection …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2716"/>
        <source>Serielle Verbindung</source>
        <translation>Serial Connection</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2697"/>
        <location filename="../qml/Main.qml" line="2733"/>
        <location filename="../qml/Main.qml" line="2842"/>
        <location filename="../qml/Main.qml" line="2897"/>
        <source>Port</source>
        <translation>Port</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2739"/>
        <location filename="../qml/Main.qml" line="2899"/>
        <source>Baudrate</source>
        <translation>Baud Rate</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2749"/>
        <source>Keine seriellen Ports gefunden.</source>
        <translation>No serial ports found.</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="529"/>
        <location filename="../qml/Main.qml" line="2827"/>
        <source>Shell</source>
        <translation>Shell</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="529"/>
        <location filename="../qml/Main.qml" line="2827"/>
        <source>SSH</source>
        <translation>SSH</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="529"/>
        <location filename="../qml/Main.qml" line="2827"/>
        <source>Seriell</source>
        <translation>Serial</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="338"/>
        <source>SSH …</source>
        <translation>SSH …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="345"/>
        <source>Seriell …</source>
        <translation>Serial …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1579"/>
        <location filename="../qml/Main.qml" line="1667"/>
        <location filename="../qml/Main.qml" line="1819"/>
        <source>Neue SSH-Verbindung …</source>
        <translation>New SSH Connection …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1593"/>
        <location filename="../qml/Main.qml" line="1677"/>
        <location filename="../qml/Main.qml" line="1821"/>
        <source>Verbindungen verwalten …</source>
        <translation>Manage Connections …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1849"/>
        <source>Unterhaltung fortsetzen: gar nicht</source>
        <translation>Continue conversation: not at all</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1850"/>
        <source>Unterhaltung fortsetzen: jüngste im Verzeichnis</source>
        <translation>Continue conversation: most recent in directory</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1851"/>
        <source>Unterhaltung fortsetzen: Auswahl beim Start</source>
        <translation>Continue conversation: pick at startup</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1852"/>
        <source>Unterhaltung fortsetzen: gemeldete Sitzung</source>
        <translation>Continue conversation: reported session</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1882"/>
        <source>SFTP: %1</source>
        <translation>SFTP: %1</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1892"/>
        <source>Aktives Fenster</source>
        <translation>Active Window</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1893"/>
        <source>Fenster gruppieren …</source>
        <translation>Group Window …</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1897"/>
        <source>Fenster zu Gruppe: %1</source>
        <translation>Window to Group: %1</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1901"/>
        <source>Fenster aus Gruppe nehmen</source>
        <translation>Remove Window from Group</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1910"/>
        <source>Gruppe nach oben: %1</source>
        <translation>Move Group Up: %1</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1913"/>
        <source>Gruppe nach unten: %1</source>
        <translation>Move Group Down: %1</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2224"/>
        <source>Unterhaltung fortsetzen</source>
        <translation>Continue conversation</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2227"/>
        <source>Gar nicht</source>
        <translation>Not at all</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2227"/>
        <source>Jüngste im Verzeichnis</source>
        <translation>Most recent in directory</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2228"/>
        <source>Auswahl beim Start</source>
        <translation>Pick at startup</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2228"/>
        <source>Gemeldete Sitzung</source>
        <translation>Reported session</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2641"/>
        <source>⟫ Eingabe geht an ALLE Sessions — Strg/Cmd+Umschalt+B zum Beenden</source>
        <translation>⟫ Input goes to ALL sessions — Ctrl/Cmd+Shift+B to stop</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2668"/>
        <source>SSH-Verbindung</source>
        <translation>SSH Connection</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2693"/>
        <location filename="../qml/Main.qml" line="2838"/>
        <source>Host</source>
        <translation>Host</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2695"/>
        <location filename="../qml/Main.qml" line="2840"/>
        <source>Benutzer</source>
        <translation>User</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2699"/>
        <location filename="../qml/Main.qml" line="2844"/>
        <source>Identity-Datei</source>
        <translation>Identity File</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2703"/>
        <location filename="../qml/Main.qml" line="2910"/>
        <source>Passwort/Schlüssel werden im Terminal abgefragt (System-ssh).</source>
        <translation>Password/key will be requested in the terminal (system ssh).</translation>
    </message>
    <message>
        <source>Agent-Steuerung</source>
        <translation type="vanished">Agent Control</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1652"/>
        <source>Neue Session: %1</source>
        <translation>New session: %1</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1659"/>
        <source>Session-Typ wählen</source>
        <translation>Select session type</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2043"/>
        <location filename="../qml/Main.qml" line="2243"/>
        <source>MCP-Server: an (127.0.0.1:%1)</source>
        <translation>MCP Server: on (127.0.0.1:%1)</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="2044"/>
        <location filename="../qml/Main.qml" line="2244"/>
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
        <location filename="../qml/Main.qml" line="1486"/>
        <location filename="../qml/Main.qml" line="1834"/>
        <source>Kopieren</source>
        <translation>Copy</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="1493"/>
        <location filename="../qml/Main.qml" line="1835"/>
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
        <location filename="../qml/Main.qml" line="2211"/>
        <source>Neue Agent-Session …</source>
        <translation>New Agent Session …</translation>
    </message>
    <message>
        <source>Hilfe</source>
        <translation type="vanished">Help</translation>
    </message>
    <message>
        <location filename="../qml/Main.qml" line="619"/>
        <location filename="../qml/Main.qml" line="1614"/>
        <location filename="../qml/Main.qml" line="1858"/>
        <location filename="../qml/Main.qml" line="2054"/>
        <location filename="../qml/Main.qml" line="3160"/>
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
        <location filename="../qml/Main.qml" line="586"/>
        <location filename="../qml/Main.qml" line="2079"/>
        <source>Standard-Shell</source>
        <translation>Default Shell</translation>
    </message>
</context>
<context>
    <name>PrefsWindow</name>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="21"/>
        <location filename="../qml/PrefsWindow.qml" line="249"/>
        <source>Einstellungen</source>
        <translation>Settings</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="101"/>
        <source>Allgemein</source>
        <translation>General</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="102"/>
        <source>Erscheinungsbild</source>
        <translation>Appearance</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="103"/>
        <source>Terminal</source>
        <translation>Terminal</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="104"/>
        <source>Eingabe</source>
        <translation>Input</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="105"/>
        <source>Agenten &amp; MCP</source>
        <translation>Agents &amp; MCP</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="106"/>
        <source>Tastenkürzel</source>
        <translation>Keyboard shortcuts</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="107"/>
        <source>Verbindungen</source>
        <translation>Connections</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="108"/>
        <location filename="../qml/PrefsWindow.qml" line="189"/>
        <source>Secrets-Vault</source>
        <translation>Secrets Vault</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="109"/>
        <location filename="../qml/PrefsWindow.qml" line="190"/>
        <source>Erweiterungen</source>
        <translation>Extensions</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="127"/>
        <source>%1 Abos</source>
        <translation>%1 subscriptions</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="134"/>
        <source>%1 geändert</source>
        <translation>%1 changed</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="141"/>
        <source>gesperrt</source>
        <translation>locked</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="170"/>
        <source>Design</source>
        <translation>Theme</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="171"/>
        <source>Sprache</source>
        <translation>Language</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="172"/>
        <source>Vor dem Beenden nachfragen</source>
        <translation>Ask before quitting</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="173"/>
        <source>Quake-Modus</source>
        <translation>Quake mode</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="174"/>
        <source>Farbschema (Dunkel)</source>
        <translation>Color scheme (dark)</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="175"/>
        <source>Farbschema (Hell)</source>
        <translation>Color scheme (light)</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="176"/>
        <source>Schriftart</source>
        <translation>Font</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="177"/>
        <source>Schriftgröße</source>
        <translation>Font size</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="178"/>
        <source>Ligaturen</source>
        <translation>Ligatures</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="179"/>
        <source>GPU-Glyph-Atlas</source>
        <translation>GPU glyph atlas</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="180"/>
        <source>Standard-Shell</source>
        <translation>Default Shell</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="181"/>
        <source>Auswahl automatisch kopieren</source>
        <translation>Copy on select</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="182"/>
        <source>Rechtsklick fügt ein</source>
        <translation>Right-click pastes</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="183"/>
        <source>Vor mehrzeiligem Einfügen warnen</source>
        <translation>Warn before multiline paste</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="184"/>
        <source>Agenten beim Start wiederherstellen</source>
        <translation>Restore agents on start</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="185"/>
        <source>Unterhaltung fortsetzen</source>
        <translation>Continue conversation</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="186"/>
        <source>Benachrichtigungen</source>
        <translation>Notifications</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="187"/>
        <source>MCP-Server</source>
        <translation>MCP server</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="188"/>
        <source>Verbindungsprofile</source>
        <translation>Connection profiles</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="272"/>
        <source>Suchen — z. B. „Ligaturen“</source>
        <translation>Search — e.g. “Ligatures”</translation>
    </message>
    <message>
        <location filename="../qml/PrefsWindow.qml" line="452"/>
        <source>Alles wirkt sofort.</source>
        <translation>Everything applies instantly.</translation>
    </message>
</context>
<context>
    <name>Shells</name>
    <message>
        <location filename="../src/core/Session.cpp" line="32"/>
        <location filename="../src/core/ShellRegistry.cpp" line="105"/>
        <source>Eingabeaufforderung (Clink)</source>
        <translation>Command Prompt (Clink)</translation>
    </message>
    <message>
        <location filename="../src/core/Session.cpp" line="54"/>
        <location filename="../src/core/ShellRegistry.cpp" line="88"/>
        <source>Eingabeaufforderung</source>
        <translation>Command Prompt</translation>
    </message>
</context>
<context>
    <name>SplitNode</name>
    <message>
        <location filename="../qml/SplitNode.qml" line="303"/>
        <source>Scrollback durchsuchen …</source>
        <translation>Search scrollback …</translation>
    </message>
    <message>
        <location filename="../qml/SplitNode.qml" line="321"/>
        <source>0</source>
        <translation>0</translation>
    </message>
    <message>
        <location filename="../qml/SplitNode.qml" line="369"/>
        <source>-Klick zum Öffnen: </source>
        <translation>-click to open: </translation>
    </message>
</context>
<context>
    <name>WindowModel</name>
    <message>
        <location filename="../src/viewmodels/WindowModel.cpp" line="48"/>
        <location filename="../src/viewmodels/WindowModel.cpp" line="48"/>
        <source>Fenster %1</source>
        <translation>Window %1</translation>
    </message>
</context>
<context>
    <name>qtmux::SftpClient</name>
    <message>
        <location filename="../src/viewmodels/SftpClient.cpp" line="37"/>
        <location filename="../src/viewmodels/SftpClient.cpp" line="37"/>
        <source>Kein Host angegeben.</source>
        <translation>No host given.</translation>
    </message>
    <message>
        <location filename="../src/viewmodels/SftpClient.cpp" line="58"/>
        <location filename="../src/viewmodels/SftpClient.cpp" line="58"/>
        <source>Verbinde mit %1 …</source>
        <translation>Connecting to %1 …</translation>
    </message>
    <message>
        <location filename="../src/viewmodels/SftpClient.cpp" line="61"/>
        <location filename="../src/viewmodels/SftpClient.cpp" line="61"/>
        <source>sftp konnte nicht gestartet werden.</source>
        <translation>Could not start sftp.</translation>
    </message>
    <message>
        <location filename="../src/viewmodels/SftpClient.cpp" line="68"/>
        <location filename="../src/viewmodels/SftpClient.cpp" line="68"/>
        <source>Zeitüberschreitung beim Verbindungsaufbau.</source>
        <translation>Connection timed out.</translation>
    </message>
    <message>
        <location filename="../src/viewmodels/SftpClient.cpp" line="111"/>
        <location filename="../src/viewmodels/SftpClient.cpp" line="111"/>
        <source>Verbunden.</source>
        <translation>Connected.</translation>
    </message>
    <message numerus="yes">
        <location filename="../src/viewmodels/SftpClient.cpp" line="131"/>
        <location filename="../src/viewmodels/SftpClient.cpp" line="131"/>
        <source>%n Einträge</source>
        <translation>
            <numerusform>%n entry</numerusform>
            <numerusform>%n entries</numerusform>
        </translation>
    </message>
    <message>
        <location filename="../src/viewmodels/SftpClient.cpp" line="135"/>
        <location filename="../src/viewmodels/SftpClient.cpp" line="135"/>
        <source>Verzeichniswechsel fehlgeschlagen: %1</source>
        <translation>Could not change directory: %1</translation>
    </message>
    <message>
        <location filename="../src/viewmodels/SftpClient.cpp" line="144"/>
        <location filename="../src/viewmodels/SftpClient.cpp" line="144"/>
        <source>Download fehlgeschlagen.</source>
        <translation>Download failed.</translation>
    </message>
    <message>
        <location filename="../src/viewmodels/SftpClient.cpp" line="145"/>
        <location filename="../src/viewmodels/SftpClient.cpp" line="146"/>
        <location filename="../src/viewmodels/SftpClient.cpp" line="145"/>
        <location filename="../src/viewmodels/SftpClient.cpp" line="146"/>
        <source>Heruntergeladen: %1</source>
        <translation>Downloaded: %1</translation>
    </message>
    <message>
        <location filename="../src/viewmodels/SftpClient.cpp" line="149"/>
        <location filename="../src/viewmodels/SftpClient.cpp" line="149"/>
        <source>Upload fehlgeschlagen.</source>
        <translation>Upload failed.</translation>
    </message>
    <message>
        <location filename="../src/viewmodels/SftpClient.cpp" line="150"/>
        <location filename="../src/viewmodels/SftpClient.cpp" line="150"/>
        <source>Hochgeladen: %1</source>
        <translation>Uploaded: %1</translation>
    </message>
    <message>
        <location filename="../src/viewmodels/SftpClient.cpp" line="172"/>
        <location filename="../src/viewmodels/SftpClient.cpp" line="172"/>
        <source>Verbindung fehlgeschlagen.</source>
        <translation>Connection failed.</translation>
    </message>
    <message>
        <location filename="../src/viewmodels/SftpClient.cpp" line="174"/>
        <location filename="../src/viewmodels/SftpClient.cpp" line="174"/>
        <source>Verbindung geschlossen.</source>
        <translation>Connection closed.</translation>
    </message>
    <message>
        <location filename="../src/viewmodels/SftpClient.cpp" line="208"/>
        <location filename="../src/viewmodels/SftpClient.cpp" line="208"/>
        <source>Lade herunter: %1 …</source>
        <translation>Downloading: %1 …</translation>
    </message>
    <message>
        <location filename="../src/viewmodels/SftpClient.cpp" line="220"/>
        <location filename="../src/viewmodels/SftpClient.cpp" line="220"/>
        <source>Lade hoch: %1 …</source>
        <translation>Uploading: %1 …</translation>
    </message>
</context>
</TS>

# Workorder: Online-Update-Epic — Anteil QTmux

> Auftraggeber: Orchestrator-Session (QTmux #23, `_ClaudeWorkspace`), 2026-08-02.
> Masterplan: `/Users/nobser/Projects/_ClaudeWorkspace/online-update-masterplan.md` (maßgeblich bei Widersprüchen).
> Jira: **QTMUX-125** existiert (aktualisieren, nicht neu anlegen), dual. Autarke Auftragsgrundlage.

## Rolle im Epic

QTmux ist reiner Desktop-Update-Konsument. Es teilt KEINEN Code über Submodule mit MacPCAN/RAFTNG → der Update-Core wird **byte-identisch vendiert** (libvterm-Muster). Kein CAN, keine Firmware.

## Arbeitspakete (E im Masterplan)

1. **Vendoring**: `third_party/updater/` = byte-identische Kopie von `../MacPCAN/src/update/` (inkl. `ed25519/`); neues CMake-Target `qtmux_updater` (STATIC, Qt6::Core+Network) — **NICHT** in `qtmux_core` (bleibt Qt6::Core-only, CLAUDE.md-Regel). `tools/check-updater-sync.sh` (SHA-256-Abgleich gegen MacPCAN) + `third_party/updater/UPSTREAM.md` (gepinnter MacPCAN-Commit).
2. **ViewModel**: `src/viewmodels/UpdateViewModel.{h,cpp}` (Q_PROPERTYs busy/updateAvailable/remoteVersion/notes[DE/EN nach `ui/language`]/downloadProgress/state/lastError; Q_INVOKABLEs checkNow/download/skipVersion/launchInstaller). Registrierung als **Context-Property** in `src/app/main.cpp` (CLAUDE.md-Regel: kein `qmlRegisterSingletonInstance` in URI „QTmux"). Startup-Hook dort (Throttle via `UpdatePolicy`).
3. **QML**: `qml/dialogs/UpdateDialog.qml` (Notes, Progress, Buttons, SmartScreen/Gatekeeper-Hinweis); Hilfe-Menü-Action vor `actAbout` (`qml/Main.qml` ~2952) + Command-Palette-Eintrag; Checkbox in `qml/prefs/CatAllgemein.qml`.
4. **Settings**: Keys `update/*` in `src/viewmodels/SettingsIo.cpp` (~Zeile 96) + `tests/tst_settingsio.cpp` erweitern; i18n DE-Quellsprache + `qtmux_en.ts` + `tst_i18n` grün; neuer `tests/tst_updater.cpp` gegen file://-Fixtures.
5. **Opportunistisch**: generiertes `qtmux_version.h` aus `PROJECT_VERSION` (`cmake/Version.h.in`) → ersetzt Hardcodes in `src/app/main.cpp:256` + `src/server/McpServer.cpp:413`; übrige Bump-Stellen bleiben (CLAUDE.md:337).

## Abhängigkeiten (WICHTIG)

| Richtung | Partner | Gegenstand |
|---|---|---|
| ⬅ EINGEHEND (BLOCKER) | **MacPCAN** (`../MacPCAN`, Session #12) | Der zu vendierende `src/update/`-Core muss existieren + gepusht sein. **E erst starten, wenn MacPCANUpdater fertig ist.** Danach byte-identisch kopieren; bei künftigen Core-Änderungen re-vendieren (Sync-Skript) |
| ⬅ EINGEHEND | **Owner** | Ed25519-Produktions-Public-Key (kommt über MacPCANs `UpdateKeys.hpp`) — bis dahin Test-Key |
| ➡ AUSGEHEND | — | Einbahnstraße: `third_party/updater/` NIE lokal editieren (sonst Drift); QTmux-spezifische Logik gehört ins ViewModel/QML |

## Nachgemeldete Anforderungen (eingetragen, NICHT begonnen)

### Proxy-Unterstützung (Owner, 2026-08-03)

**Anforderung:** „Im Firmenumfeld müssen auch Netzwerk-Proxies unterstützt werden, was eine
Konfiguration erfordert."

**Proxy-Unterstützung: appupdate-Proxy-Konfiguration nachvendieren, sobald MacPCAN sie
liefert** (SHA-256-Abgleich via `check-updater-sync.sh`, `UPSTREAM.md` nachziehen).
**QTmux-Anteil:** Settings-Keys `update/proxy*` in `SettingsIo.cpp` + `tst_settingsio`,
Proxy-Abschnitt in `qml/prefs/CatAllgemein.qml`, Auth-Abfrage im `UpdateDialog.qml`,
`UpdateViewModel`-Properties, i18n DE→EN + `tst_i18n`.

⛔ **Keine eigene Proxy-Implementierung anfangen.** Der Mechanismus wird kanonisch in der
Shared-Lib **MacPCANUpdater (`appupdate`)** gebaut — MacPCAN ist der Hub und liefert das
Konzept. Drei Apps mit drei verschiedenen Proxy-Wegen sind genau das, was damit vermieden
wird.

**Warum diese Schnittlinie:** Die Lib bleibt **GUI- und QSettings-frei** (bestehender
Vertrag). Dialog und Persistenz gehören deshalb in die App, das **Netzwerkverhalten** in die
Lib.

🔑 **QTmux-Besonderheit gegenüber RAFTNG (dort Submodul): wir erben über VENDORING.** Beim
Nachziehen gilt deshalb zusätzlich — `third_party/updater/` bleibt **byte-identisch** zur
MacPCAN-Quelle (nie lokal editieren, Einbahnstraße s. o.), **`tools/check-updater-sync.sh`
muss danach wieder grün sein**, und der gepinnte MacPCAN-Commit in
`third_party/updater/UPSTREAM.md` wird nachgezogen. Ein Proxy-Fix gehört also nach MacPCAN,
nicht hierher.

**Nicht betroffen:** Die **CI ändert sich nicht** (github-hosted; QTmux ist als einziges
Repo der Familie public). Der Update-Zyklus ist mit **v1.8.0 live verifiziert** — Proxy ist
eine **Erweiterung, kein Defekt**.

**Reihenfolge:** eingehender Blocker wie bei Paket 1 — erst wenn MacPCAN die
Proxy-Konfiguration gepusht hat, wird re-vendiert; der QTmux-Anteil hängt an den dann
vorhandenen Lib-Schnittstellen.

### Zuarbeit an MacPCAN: QTmux-Anforderungen an die Lib-API (2026-08-03)

> Auftrag des Owners: Anforderungen **vor** dem Lib-Konzept liefern, damit die API einmal
> passt. Kein Code, keine eigene Implementierung. Alles unten ist am Repo **gemessen**,
> nicht angenommen; Messstellen sind jeweils genannt.

#### 1 · Settings-Keys `update/proxy*`

Bestehendes Muster: `SettingsIo` führt eine **Allowlist** (`update/autoCheck`,
`skippedVersion`, `baseUrl`; `lastCheck` bewusst draußen). Neu:

| Schlüssel | Werte | Export? | Warum |
|---|---|---|---|
| `update/proxyMode` | `system` (**Vorgabe**) · `none` · `manual` | ja | `system` = `QNetworkProxyFactory::useSystemConfiguration()`. Im Firmenumfeld steht der Proxy meist per WPAD/System schon — damit läuft der Normalfall **ohne** Konfiguration. `none` muss explizit wählbar sein: eine kaputte oder unerreichbare PAC-Datei ist genau der Fall, in dem direkt funktioniert und „System" hängt |
| `update/proxyType` | `http` · `socks5` | ja | nur bei `manual` |
| `update/proxyHost`, `update/proxyPort` | Text / Zahl | ja | nur bei `manual` |
| `update/proxyUser` | Text (oft `DOMÄNE\benutzer`) | **nein** | Kein Geheimnis, aber Personenbezug. Der Export ist eine Datei, die der Anwender **weitergibt** — Proxy-Host ist harmlos, der Domänen-Benutzername gehört nicht hinein |
| `update/proxyAuth` | `none` · `session` (**Vorgabe**) · `vault` | ja | woher das Passwort kommt, s. Punkt 3 |
| `update/proxySecret` | **Name** eines Vault-Geheimnisses | ja | exakt das Muster von `ConnectionProfile::passwordSecret` — gespeichert wird der Name, nie der Wert |

**Kein Passwort-Schlüssel.** Und bewusst **kein** `proxyExcludes`/`no_proxy`: Der Updater
spricht genau **eine** Host-URL an; eine Ausnahmeliste wäre eine Einstellung ohne Fall.

#### 2 · Wie QML die Auth-Rückfrage beantwortet

🔑 **`proxyAuthenticationRequired` ist als QML-Rückfrage prinzipiell untauglich.** Qt liefert
es **synchron** aus; der `QAuthenticator*` gilt nur während des Slot-Aufrufs. Ein QML-Dialog
antwortet aber asynchron — der Slot wäre längst zurück. Es bliebe eine verschachtelte
Event-Loop, und die ist mitten in einem Netzwerk-Callback genau die Klasse Fehler, die uns
der `busy()`-Fall schon einmal gekostet hat (MacPCAN `59a9e35`).

**Gewünschter Weg — zweistufig, die Lib fragt nie:**

1. Die App setzt **vorab** einen Anmelde-Lieferanten, den die Lib im Slot synchron
   befragt. Er **antwortet nur** (aus dem RAM), er fragt nicht:
   `setProxyCredentials(user, password)` genügt; ein `std::function`-Provider tut es auch,
   solange er nicht blockieren darf.
2. Sind keine Anmeldedaten da, bricht die Anfrage ab und die Lib meldet einen
   **unterscheidbaren Fehler** (nicht nur einen Text), z. B.
   `Error::ProxyAuthenticationRequired`. QTmux öffnet **daraufhin** den Dialog, füllt über
   Schritt 1 nach und **wiederholt** den Aufruf.

⚠️ **Der typisierte Fehler ist für uns nicht Kosmetik, sondern Owner-Regel:** Fehler des
stillen Start-Checks bleiben **still** (`main.cpp`, 3 s nach dem Start). „Netz weg" darf
niemanden stören — „Proxy will Anmeldung" muss QTmux dagegen erkennen können, um beim
**nächsten manuellen** Check zu fragen. An einem Fehler-String ist das nicht
unterscheidbar, erst recht nicht übersetzt.

⚠️ **Genau EINMAL versuchen, dann aufgeben — kein Wiederholen mit denselben Daten.** Qt ruft
`proxyAuthenticationRequired` bei falschem Passwort erneut; in einer AD-Umgebung sperrt das
nach wenigen Versuchen das **Domänen-Konto**. Dieselbe Lektion steckt bereits in QTmux beim
SSH-Passwort-Auto-Fill („genau einmal senden — kein Lockout"). Die Lib sollte das
erzwingen, nicht jeder App überlassen.

#### 3 · Wo die Anmeldedaten liegen — Bewertung

**Empfehlung: Sitzungsspeicher als Vorgabe, Vault als Wahl. Keychain nicht.**

- **Klartext in `SettingsIo`: ausgeschlossen.** Das sind Domänen-Anmeldedaten, und die
  Export-Datei ist zum Weitergeben gedacht.
- **Keychain: abgelehnt.** QTmux hat bewusst **keine externen Abhängigkeiten**; ein
  Keychain bräuchte drei Implementierungen (Security.framework · CredMan/DPAPI ·
  libsecret/DBus), und der Linux-Teil endete wie beim `SleepInhibitor` als **Stub** — also
  ausgerechnet dort nichts, wo Firmen-Linux steht.
- **Vault (`SecretsVault`): möglich, aber nicht als Vorgabe.** Er verwaltet dieselbe Klasse
  Geheimnis (SSH-Passwörter) schon, mit dem `passwordSecret`-Namensmuster — ein zweiter
  Speicherort widerspräche „je Sachverhalt EINE Stelle". **Aber gemessen:** er ist
  master-passwortgeschützt und startet **gesperrt** (`isUnlocked()` = false). Der stille
  Start-Check dürfte also nach dem Master-Passwort fragen — genau das verbietet die
  Owner-Regel.
- **Sitzungsspeicher (nur RAM, ein Programmlauf): Vorgabe.** Überlebt nichts, landet
  nirgends, und trifft den Firmenfall gut genug — einmal pro Sitzung fragen ist zumutbar,
  ein gesperrtes Konto nicht.

Daraus die **Anforderung an die Lib**: sie **persistiert grundsätzlich nichts** und kennt
weder Vault noch QSettings. Sie hält die Anmeldedaten nur für die Laufzeit ihrer Instanz und
bietet ein **`clearProxyCredentials()`** — QTmux ruft es beim Sperren des Vaults und bei
`proxyAuth = none`.

#### 4 · Nimmt `check-updater-sync.sh` neue Dateien mit?

**Das Skript: ja, automatisch — keine Liste zu pflegen.** Es bildet die Dateiliste aus
**beiden** Bäumen (`find` über vendiert *und* upstream, `sort -u`); eine upstream **neue**
Datei erscheint als `FEHLT VENDIERT (upstream neu)`, setzt Exit 1 und wird von `--update`
angelegt. Auch eine upstream **gelöschte** fällt auf (`NUR VENDIERT`).

⚠️ **Die Handpflege sitzt woanders — in `CMakeLists.txt`.** Das Target `qtmux_updater`
listet jede Quelldatei **einzeln** auf (`CMakeLists.txt:157–172`). Eine neu vendierte
`ProxyConfig.cpp` wäre also kopiert und sync-grün, aber **nicht übersetzt** — je nach
Aufbau bleibt das bis zum Linkfehler unsichtbar oder, schlimmer, bis zur Laufzeit still.
**Bitte in der Ankündigung des Lib-Pakets die Dateiliste mitliefern**, dann ziehen wir
CMake im selben Schritt nach. (Ein `GLOB` wäre die falsche Antwort: er verdeckt genau die
Drift, die der Wächter sichtbar machen soll.)

## Infrastruktur-Fakten

- Update-Basis-URL: **`https://nobser.de/updates/qtmux/manifest.json`** (statisch, T-Online). Assets liegen zusätzlich auf GitHub Releases (`RealNobser/QTmux`) — Manifest kann auf beides zeigen (Owner-Entscheidung bei Umsetzung; Default T-Online-Webspace).
- Owner-Entscheidungen: Download + geführte Installation (kein Silent-Self-Replace); Check beim Start max. 1×/Tag + manuell, abschaltbar; Downgrade zulassen mit Warnung.
- QTmux-Installer existieren alle (`installer/build-dmg.sh`, `build-msi.ps1`, `build-appimage.sh`) → alle drei OS-Keys von Anfang an belegbar.
- Diese Session (QTmux-Worker) ODER die Orchestrator-Session setzt E um — vor Beginn `list_sessions`/Arbeitsverzeichnis prüfen, um Doppelarbeit zu vermeiden.

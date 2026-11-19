# .github/ — CI (GitHub Actions)

> Schicht-Datei: Claude Code lädt sie, wenn in `.github/` gearbeitet wird. **Wer einen roten
> Lauf deutet oder eine Fertigmeldung auf CI-Ergebnisse stützt, liest sie vorher auch von
> anderswo.** Die Absätze stammen wörtlich aus dem CI-Abschnitt der Wurzel-`CLAUDE.md`
> (ausgelagert 2026-11-19); dort bleibt die Kurzbeschreibung von `.github/workflows/ci.yml`.

🔴 **Linux-Runner fest auf `ubuntu-24.04`, nicht `ubuntu-latest` (2026-11-03).** Das AppImage
bündelt die glibc nicht, seine Mindestanforderung folgt dem Runner-Image. `ubuntu-latest` sprang
still auf 26.04 und hob sie von GLIBC 2.38 auf 2.43 — auf Ubuntu 24.04 LTS wäre das Image nicht
mehr gestartet, und per Selbstersetzung wäre die Installation kaputt gewesen. CI und Bau blieben
grün, aufgefallen nur an `Image:` im Job-Log und am Inhaltsvergleich gegen das Vorrelease.
Messgerät: höchstes `GLIBC_` in `usr/lib`/`usr/bin` des extrahierten Images. Das Anheben ist eine
Owner-Entscheidung, keine Runner-Nebenwirkung.
🔑 **Release-AppImage = GitHub-Release-Asset, KEIN Actions-Artefakt (QTMUX-136,
2026-09-28).** Die Artefakt-Quota ist **account-weit** (alle Repos von RealNobser) und war
am 2026-09-28 voll (EmbyStudios 0.0.4-Tag-Lauf scheiterte dreimal allein am Upload);
Release-Assets zählen nicht dagegen. Mechanik nach EmbyStudio-Vorlage (EMB-34): eigener
Job **`linux-release`** — nur Tag-Pushes `v*`, `needs: build`, **`contents: write` nur
dort** (Workflow-Standard `contents: read`; `permissions` kennt keine Ausdrücke, darum ein
eigener Job), Fork-Riegel, nur `GITHUB_TOKEN`. Er prüft Tag = `v<CMakeLists-Version>`,
baut Release ohne Tests, packt und hängt das Image per
[tools/rc_release_asset.py](tools/rc_release_asset.py) an das Release des Tags: fehlt es,
entsteht ein **Draft**; ein **veröffentlichtes** Release und ein Tag auf **fremdem Commit**
werden verweigert, danach Rücklesen (Größe + sha256-Digest). Selbsttest ohne Netz
[tools/test_rc_release_asset.py](tools/test_rc_release_asset.py) läuft im Linux-Matrix-Job
(Mutationsprobe: beide Verweigerungen aus → rot). Eine AppImage-**Abnahme** in der CI hat
QTmux nicht (anders als EmbyStudio) — die Build-ID misst das Release-Rezept am
heruntergeladenen Asset. Probe: Wegwerf-Tag `appimage-probe-1` (Lauf `36484293842`,
4/4 grün; Draft mit Asset, sha256 in Job-Log = GitHub-Digest = Download, Build-ID
`1.9.5+2957f31` im extrahierten Image ASCII + UTF-16, kein `-dirty`; danach Tag + Draft
gelöscht und Zulassung entfernt) — zum Wiederholen
`appimage-probe-*` für EINEN Commit in `on.push.tags`, im Job-`if` und im `case` von
„Version und Tag" zulassen.
🔑 **Kein TMPDIR-Umweg im macOS-Job (QTMUX-148, 2026-10-16).** Apple clang 21 lässt bei
einem Aufruf mit **mehreren** `-arch` je übersetzter Datei und Architektur einen leeren Ordner
im TMPDIR liegen (lokal gemessen: eine Architektur → 0, zwei → 2 je Datei); die Familien-Repos
mit universalen Mac-Bauten leiten TMPDIR darum beim Konfigurieren/Bauen nach `runner.temp`
um. QTmux baut single-arch (jeder Compile-Befehl trägt genau ein `-arch`; ein kompletter
lokaler Bau mit eigenem leerem TMPDIR legt 0 Einträge an), und die GitHub-gehosteten Runner
sind je Job eine eigene Maschine (`runner_name` je Job verschieden, Gruppe „GitHub Actions“)
— die Umleitung entfällt. Statt ihrer stehen Belegzeilen im Job: `BUILD-TMPDIR-START`
(Ausgangsstand samt Runner-Name und Image), nach dem Bau `BUILD-TMPDIR`, `BUILD-TMPDIR-NEW`
(Namen der neu entstandenen leeren Ordner) und `BUILD-ARCH`. ⚠️ **Eine Differenz allein ist
kein Befund:** Das Image hat schon vor dem Bau 72 bzw. 95 leere Ordner (Läufe `38879028974`,
`38880025712`), und während des Baus legen macOS-Systemdienste weitere an (gemessen +8 bzw.
+3, benannt `com.apple.fskit.fskit_agent`, `com.apple.geoanalyticsd`,
`com.apple.intelligencetasksd`). Erst Compiler-Ordnernamen in `BUILD-TMPDIR-NEW` oder zwei
Architekturen in `BUILD-ARCH` verlangen die Umleitung (Falle dabei: `mktemp -d`
ohne Vorlage ignoriert unter macOS TMPDIR → `mktemp -d "$TMPDIR/x.XXXXXX"`; den Test-Schritt
NICHT umleiten, AF_UNIX-Pfade sind auf ~104 Byte begrenzt).
**Windows-Tests sind seit `db52b41` blockierend** — vorher lief der Schritt mit
`continue-on-error`, weil `test_pty` dort umgebungsbedingt fällt; damit waren aber auch alle
übrigen Tests wirkungslos, eine echte Regression hätte den Job nie rot gemacht. Jetzt wird nur
`test_pty` per `-E` ausgenommen. Node-20-Abkündigung ist abgearbeitet (checkout v5,
upload-artifact v6, `setup-msvc-dev` statt `ilammy`, Ninja kommt vom Image); die beiden
Dritt-Actions (`setup-msvc-dev`, `install-qt-action`) sind auf **Commit-SHAs gepinnt** —
Tags/Branches sind verschiebbar; das Anhebe-Rezept steht als Kommentar in der `ci.yml`.
> ⚠️ **Zweiter beobachteter Flake (2026-08-02): macOS-Build bricht bei
> `qmltyperegistrar` mit `qt6qtmux_metatypes.json:: Failed to parse JSON: 7 illegal
> number`.** Die Datei ist **generiert** — derselbe Commit-Inhalt baut im Folgelauf
> sauber, lokal ist die JSON jederzeit parsebar. Also dieselbe Regel wie unten: nicht
> als Regression lesen, **erst wiederholen**. (Wäre es echt, zeigte es sich lokal
> reproduzierbar an einer moc-JSON.)

> ⚠️ **Sporadisch: `test_doc_duplicates` fällt auf Windows mit `0xc0000142`**
> (STATUS_DLL_INIT_FAILED). Das ist **kein** Prüfbefund — der Prozess (`cmake -P`) startet gar
> nicht erst, die Doku wird nie gelesen. Zweimal gesehen (Läufe `30635817273`, `31214375537`),
> danach jeweils wieder grün. Fällt erst seit der Blockier-Umstellung überhaupt auf. Also: nicht
> als Regression lesen und nicht in der CheckDocDuplicates.cmake suchen — **erst wiederholen**.
> Bleibt es reproduzierbar, ist die Spur Ressourcenerschöpfung durch die davor laufenden
> `run_detached.ps1`-Tests, nicht der Doku-Wächter selbst.

> 🔑 **Wie man einen CI-Flake BEWEIST, statt ihn zu vermuten** (an `3b63f00` durchgespielt,
> dritter Windows-Fehlschlag dieser Art): Ein roter Lauf und ein grüner Folgelauf sind erst
> dann ein Beleg, wenn der Folgecommit **denselben Code** trägt —
> `git diff --name-only <rot>..<gruen>` muss ausschließlich Doku nennen. Bei `3b63f00`→`923e8fc`
> war der Unterschied genau `CLAUDE.md`, und `923e8fc` ist auf allen drei Plattformen grün:
> damit ist der rote Lauf erledigt, ohne ihn zu wiederholen.
> ⚠️ **Auf das Log darf man dabei nicht bauen** — schon einen Tag später antwortete
> `gh run view --job <id> --log` mit `log not found`, während `gh run view <lauf> --json jobs`
> Status und Fehlschritt weiter lieferte. Wer erst das Log sucht, steht ohne Diagnose da; der
> Commit-Vergleich ist unabhängig davon. Und: `gh` braucht das Repo-Verzeichnis oder
> `-R RealNobser/QTmux`, sonst „failed to determine base repo".

> ⚠️ **Windows-Job stirbt MITTEN im Build mit `##[error]Process completed with exit code -1`**
> (von **RAFTNG** gemeldet 2026-08-07, geteilte Runner-Infrastruktur — bei uns noch nicht
> aufgetreten, hier bewusst vorab notiert). **Kein Compilerfehler, kein `FAILED`, kein
> „ninja: build stopped"** — der Lauf endet einfach an einem beliebigen Ziel, und das
> getroffene Ziel hat **keinen Bezug zum Diff**. Genau das verführt dazu, im eigenen Diff zu
> suchen: Es sieht aus, als habe die letzte Änderung etwas zerlegt.
> 🔑 **Regel: erst `gh run rerun <id> --failed`, dann im eigenen Diff suchen.** RAFTNG hat es
> per Rerun **auf demselben Commit** als Runner-Ursache belegt; die umgekehrte Reihenfolge
> kostet einen Abend.
> ⚠️ **Nicht mit dem Ausfall vom 2026-08-06 verwechseln** (Git-Lektionen unten) — der sieht
> nur oberflächlich ähnlich, die Mechanik ist eine andere: Dort entstand **gar kein Lauf**
> (GitHub-Störung, Webhooks gedrosselt), hier stirbt ein **laufender Job** mittendrin. Beide
> teilen allein die Lehre, zuerst außerhalb des eigenen Codes zu suchen; das Messmittel ist
> ein anderes — dort die Statuspage, hier der Rerun.
>
> 🔑 **Zwei Fehlerbilder auf geteilten Runnern — nur EINES ist Infrastruktur** (RAFTNG-
> Präzisierung 2026-08-07). Die Unterscheidung entscheidet, ob der Rerun das richtige oder
> das **zerstörerische** Werkzeug ist:
> | Bild | Deutung | Erster Griff |
> |---|---|---|
> | `exit code -1` **mitten** im Build, kein Compilerfehler | Runner-Abbruch | `gh run rerun <id> --failed` |
> | Build **komplett**, **ein einzelner Test** rot | **echter Befund** (meist Timing unter Fremdlast) | **erst dokumentieren** — Lauf-ID, Testname, Fehlzeile — **dann** rerun |
> ⚠️ **Beim zweiten Bild entsorgt ein voreiliger Rerun den Befund**: Der grüne Wiederholungs-
> lauf überschreibt die einzige Spur, und der Fehler heißt fortan „Flake", obwohl niemand ihn
> je gelesen hat. Fix-Richtung dort: **Wartebedingung statt fester Frist**.
> 🔑 **Aber die Fix-Richtung gilt nicht überall — bei uns gemessen (2026-08-07): 17 feste
> Fristen, davon nur ~11 echte Kandidaten.** Drei Klassen, die man auseinanderhalten muss:
> 1. **Warteschleife mit Abbruchbedingung** (`for (attempt < 100 && !found) qWait(100)`, so in
>    `tst_session` an vier Stellen) — ist bereits eine Wartebedingung, nur von Hand gebaut,
>    und toleriert bis zu 10 s Fremdlast. **Kein Handlungsbedarf.**
> 2. **Positiv-Wartung** (`qWait(500)`, danach ist etwas da) — echter Kandidat.
>    `tst_session` und `tst_pty`; ⚠️ `test_pty` läuft in der CI nur auf **macOS** mit
>    (Windows/Linux per `-E` ausgenommen), die Fristen dort treffen also nur einen Runner.
> 3. **Negativ-Wartung** (`qWait(200)`, danach muss `count() == 0` sein — `tst_updateviewmodel`
>    zweimal). ⚠️ **Hier hilft „Wartebedingung statt Frist" PRINZIPIELL nicht: Auf ein
>    Nicht-Ereignis kann man nicht warten.** Und die Gefahr steht auf dem Kopf — Fremdlast
>    macht diese Tests nicht rot, sondern **still grün**: Käme das Signal erst nach 300 ms,
>    sieht der Test trotzdem „0" und bestätigt fälschlich. Ein Wächter, der unter Last blind
>    wird, ist schlimmer als einer, der flackert.
>
> 🔑 **Woran man eine BLINDE Negativprüfung erkennt — nicht am Namen und nicht an der
> Prüfung, sondern daran, WORAUF vorher gewartet wurde** (2026-08-07 über alle Tests
> gemessen, nachdem RAFTNG zwei blinde Wächter bei sich fand). Von **16** Negativprüfungen
> im Baum sind nur **zwei** blind:
> | vorher | Beispiel | Bewertung |
> |---|---|---|
> | **Anker-Ereignis** abgewartet, das definitiv kommt | `QVERIFY(runCheck(vm,…))` spinnt bis `checkFinished`, **dann** `QCOMPARE(found.count(), 0)` (4×) | **sauber** — der Vorgang ist nachweislich durch |
> | **synchroner** Aufruf | `PromptQueue`-Signale kommen im Aufruf selbst (3× in `tst_promptqueue`) | **sauber** — deterministisch |
> | **nur eine Frist** | `qWait(200); QCOMPARE(finished.count(), 0)` (2× in `tst_updateviewmodel`) | ⚠️ **blind** |
> ⚠️ **Die Kategorie NIE vom Testnamen ableiten** (RAFTNG-Messung: deren „StreamsAndStops"
> klang negativ, prüft aber nur Zustand). Lesen, was **vor** der Prüfung steht.
> 🔑 **Und genau dort, wo es blind ist, gibt es KEIN Anker-Ereignis** — der Start-Check tut ja
> nichts, also feuert auch nichts, worauf man spinnen könnte. Deshalb greifen zwei Muster
> ineinander: **Gibt es ein Anker-Ereignis, darauf warten und danach prüfen** (billig, schon
> viermal so gelöst); **gibt es keines, den Empfänger instrumentieren** — ein verbotenes
> Ereignis setzt **sofort** ein Flag, geprüft wird das Flag statt einer Frist (RAFTNGs
> Muster, fängt den Fall wann immer er eintritt statt nur im Zeitfenster).
> ✅ Mit dem Startup-Check-Paar (1.9.4) erledigt: Beide prüfen jetzt **fristfrei** auf
> `stateChanged` — ein Start setzt `Checking` synchron im Aufruf, also ist „kein Signal
> direkt danach" deterministisch (Gegenprobe: Wächter im Code entfernt → rot).

> **⚠️ `env.QT_VERSION` (6.10.3) ist bewusst gewählt — nicht blind hochziehen.**
> **Nicht 6.8.x:** dessen CMake-Config verlinkt das aus dem macOS-SDK entfernte
> **AGL-Framework** → `ld: framework 'AGL' not found` (lokal unsichtbar, Homebrew-Qt ist
> AGL-frei); Ausweichen auf `macos-13`-Runner geht nicht (werden nicht mehr zugeteilt).
> **Nicht 6.11/6.12:** aqtinstall kann die **Windows**-Arch-Metadaten nicht abrufen
> (macOS/Linux lösen sie sauber auf — der Blocker ist allein Windows). Der offizielle
> Installer bräuchte **Qt-Account-Secrets**; das Repo ist öffentlich, Fork-PRs bekommen
> keine → der Windows-Job bräche für jeden externen Beitrag.
> 🔑 Die Fehlermeldung wechselt (zuletzt `Failed to download checksum for … Updates.xml`)
> und *sieht* nach Mirror-Flattern aus. Nur der **Gegentest** entlarvt es: 3/3
> Wiederholungen scheitern, während 6.10.3 im selben Lauf durchläuft. Vor jedem Bump
> lokal prüfen: `aqt list-qt <windows|mac|linux> desktop --arch <ver>` (aqtinstall 3.3.x)
> — Fehler = Version unbrauchbar.

## Windows-Testschritt ohne Log (1.9.12)

⚠️ **Windows-Testschritt hing einmal ohne Log** (Lauf `43725522659`, 1. Versuch: 25 min im Schritt „Tests“
statt ~30 s, nach Abbruch kein Log abrufbar, HTTP 404). Die Wiederholung und alle späteren Läufe waren
grün. Ursache ungemessen. Die Jobs haben keine `timeout-minutes`, ein solcher Hänger
liefe bis zur GitHub-Grenze von 6 h (Kandidat, nicht beauftragt).

## Workflow-Eintrag `Trigger-Test`

Aus „Zustand, der nicht aus Code/Git hervorgeht“ der Wurzel-`CLAUDE.md`:

- Der Workflow-Eintrag **`Trigger-Test`** (ID `328898398`) steht bleibend in
  `gh workflow list --all`, obwohl sein Branch gelöscht ist — GitHub führt ihn, solange sein
  Lauf existiert. Bewusst nicht entfernt: Lauf `#31128515520` ist der Beleg, dass der
  `push`-Trigger funktioniert. Wirkungslos, verschwindet von selbst.

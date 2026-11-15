#pragma once

#include <QString>

#include "FileManagerReveal.h"

// Würde das System eine lokale Datei beim Öffnen AUSFÜHREN statt anzeigen? Dann fragt QTmux vor
// einem Klick auf den Link nach (QTMUX-151). Der Pfad stammt aus fremder Terminal-Ausgabe: Ein
// Agent, eine angezeigte Datei oder eine entfernte Gegenstelle kann ihn hinschreiben, und ohne
// Rückfrage startete ein Strg-/Cmd-Klick das Programm sofort.
//
// Gui-frei, die Regel selbst ist reine Zeichenketten- und Bit-Logik und darum auf jedem Host für
// alle drei Plattformen prüfbar. Im Zweifel wird gefragt: Eine Rückfrage zu viel kostet einen
// Klick, eine zu wenig startet ein fremdes Programm.
namespace ExecutableTarget {

// Was die Regel über die Datei wissen muss. `factsFor` liest es vom Dateisystem; Tests setzen es.
struct Facts {
    bool isDir = false;        // Verzeichnis (unter macOS auch ein Bündel)
    bool anyExecBit = false;   // irgendein Ausführungsrecht gesetzt (Unix); unter Windows ohne Bedeutung
    QString resolved;          // Ziel hinter symbolischen Verknüpfungen; leer, wenn nicht auflösbar
};

// Liest die Angaben für einen absoluten lokalen Pfad. Existiert er nicht, bleiben sie leer; die
// Endungsregeln gelten trotzdem.
Facts factsFor(const QString &absPath);

// Die Regel. `pathExt` ist der Inhalt der Umgebungsvariable PATHEXT (nur Windows; leer ist erlaubt).
//  - Windows: Verzeichnisse nie. Sonst jede Datei OHNE Endung (die Shell hängt beim Öffnen selbst
//    eine Programm-Endung an), jeder benannte Datenstrom (`datei:strom`), die Endungen aus PATHEXT
//    (die eingebaute Vorgabe gilt immer mit) sowie Installer, Verknüpfungen, Skripte und `.reg`.
//  - macOS: Programme, Installer und Erweiterungen nach Endung (`.app`, `.command`, `.pkg`, …),
//    auch als Verzeichnis, denn das sind dort oft Bündel. Sonst Verzeichnisse nie, und jede Datei
//    mit Ausführungsrecht.
//  - Linux: Verzeichnisse nie. Jede Datei mit Ausführungsrecht, dazu Starter, Skripte und
//    Installer nach Endung (`.desktop`, `.sh`, `.AppImage`, `.deb`, …).
// 🔴 Die Listen sind endlich. Was fehlt, öffnet ohne Rückfrage; darum gehört jeder Typ hinein,
// den ein System beim Öffnen ausführt oder installiert, und nie einer wieder heraus, nur weil
// die Rückfrage stört.
// Geprüft werden der genannte Pfad UND sein aufgelöstes Ziel: Eine Verknüpfung `liesmich.txt`
// auf ein Skript führt das Skript aus.
bool asksBeforeOpen(const QString &absPath, const Facts &facts, FileManagerReveal::Platform platform,
                    const QString &pathExt);

// Dasselbe für diesen Rechner: liest Angaben und PATHEXT selbst.
bool asksBeforeOpen(const QString &absPath);

} // namespace ExecutableTarget

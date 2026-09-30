#pragma once

#include <QList>
#include <QString>
#include <QStringList>

// „Im Finder/Explorer/Dateimanager zeigen" für erkannte Links (QTMUX-138). Gui-frei
// (nur Qt6::Core), damit die Befehlsermittlung je Plattform ohne GUI testbar ist.
//
// Sicherheitsregeln (Links stammen aus fremder Terminal-Ausgabe, z. B. eines Agenten):
//  - Der Pfad geht NUR als Element einer Argumentliste an QProcess, nie durch eine Shell.
//  - Gezeigt wird nur, was LOKAL existiert (`localPathFor`) — ein in einer SSH-Session
//    ausgegebener Remote-Pfad, den es hier nicht gibt, bleibt wirkungslos.
//  - Der Pfad ist immer ABSOLUT (beginnt mit `/`, Laufwerk oder `\\`); ein Dateiname wie
//    `-R` kann darum nie als Option des Zielprogramms gelesen werden.
namespace FileManagerReveal {

enum class Platform { MacOS, Windows, Linux };

struct Command {
    QString     program;
    QStringList arguments;
    bool operator==(const Command &o) const {
        return program == o.program && arguments == o.arguments;
    }
};

// Plattform, auf der dieser Build läuft.
Platform currentPlatform();

// Link-Ziel → absoluter lokaler Pfad, wenn es eine existierende Datei/ein Ordner ist.
// Nimmt einen Dateipfad (absolut) oder eine file://-URL; alles andere (http, mailto,
// relative Pfade, Nicht-Existierendes) ergibt einen leeren String.
QString localPathFor(const QString &target);

// Befehle in Versuchsreihenfolge für einen ABSOLUTEN, existierenden Pfad. macOS/Windows
// liefern genau einen; Linux zwei: D-Bus `FileManager1.ShowItems` (markiert die Datei)
// und als Rückfall `xdg-open <Elternordner>` (öffnet nur den Ordner). Leer, wenn `absPath`
// nicht absolut ist.
QList<Command> commands(const QString &absPath, Platform platform);

// Führt `commands(localPathFor(target), currentPlatform())` aus: erster Versuch sofort,
// weitere nur, wenn der vorige scheitert (Programm fehlt oder Exit ≠ 0). Kehrt sofort
// zurück (asynchron). false, wenn das Ziel nichts lokal Existierendes ist.
bool reveal(const QString &target);

} // namespace FileManagerReveal

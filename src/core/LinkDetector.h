#pragma once

#include <QList>
#include <QString>
#include <QUrl>

// Gui-freie Heuristik: findet in einer Terminal-Zeile anklickbare Ziele — URLs und
// existierende Dateipfade. Bewusst in qtmux_core (nur Qt6::Core), damit sie ohne GUI
// testbar ist. OSC-8-Hyperlinks laufen NICHT hierüber (die trägt die Cell direkt); der
// Detector ist der Weg für nackten Text, den ein Agent einfach ausgibt.
namespace LinkDetector {

struct Span {
    int     start  = 0;      // Zeichen-Index in der Zeile (Beginn des Treffers)
    int     length = 0;      // Länge in Zeichen
    QString target;          // öffnungsfertig: URL wie erkannt, oder absoluter Dateipfad
    enum Kind { Url, FilePath } kind = Url;
};

// Findet alle Spans in einer einzelnen (bereits zu Text zusammengesetzten) Zeile.
// Dateipfade werden gegen `cwd` aufgelöst und NUR aufgenommen, wenn die Datei wirklich
// existiert — diese Existenzprüfung ist der eigentliche Fehlalarm-Filter. Ist `cwd`
// leer, werden nur absolute/Home-Pfade geprüft. Overlappende Treffer werden zugunsten
// des zuerst (am weitesten links) beginnenden aufgelöst.
QList<Span> detect(const QString &line, const QString &cwd = QString());

// Prüft, ob `scheme` (klein) für QDesktopServices::openUrl freigegeben ist. Bewusst eng:
// KI-Output darf nicht jedes beliebige Schema starten dürfen (`javascript:`, `data:`, …).
bool isAllowedScheme(const QString &scheme);

// Beginnt der Pfad mit zwei Trennern (`\\server\share`, `//server/share`, auch gemischt, auch nach
// Punkt-Segmenten wie `/.//server`) oder mit dem NT-Präfix `\??\`? 🔴 Solche Pfade werden NIE geprüft oder geöffnet: Sie stammen aus fremder
// Terminal-Ausgabe, und schon die Existenzprüfung beim Überfahren mit der Maus baute unter Windows eine SMB-Verbindung zu diesem
// Rechner auf. Das reicht einem fremden Server, um die Windows-Anmeldung (NTLM) mitzulesen.
bool isNetworkPath(const QString &path);

// Zeigt eine `file:`-URL auf einen anderen Rechner? Jeder Host zählt, auch `localhost`
// (`toLocalFile` macht daraus `//localhost/…`). Ungültige URLs gelten ebenfalls als entfernt.
bool isRemoteFileUrl(const QUrl &url);

} // namespace LinkDetector

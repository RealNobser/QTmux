#pragma once

// qtmux::safefile — defensives Lesen einer Datei, deren Pfad NICHT von uns stammt.
//
// Anlass: Sicherheitsbefund aus RAFTNG (2026-08-06). Dort ließ ein legaler
// HTTP-Aufruf mit `{"path": "/dev/zero"}` den Prozess in zwei Sekunden von 17 MB
// auf 30,4 GB wachsen — keine Parser-Lücke, sondern ein `QFile::readAll()` auf
// einen vom Aufrufer benannten Pfad, ohne zu prüfen, ob es überhaupt eine
// **reguläre** Datei ist. Danach war die Anwendung blockiert (der Event-Loop
// steckte im Lesen) und starb ohne Crashreport.
//
// 🔑 DIE ENTSCHEIDENDE MESSUNG (2026-08-06, macOS, mit QFileInfo geprüft):
//
//     /dev/zero      isFile=0  size=0
//     /dev/urandom   isFile=0  size=0
//     ein FIFO       isFile=0  size=0
//     /etc/hosts     isFile=1  size=215
//
// Eine reine GRÖSSENPRÜFUNG schützt also **nicht**: `size()` liefert bei
// Gerätedateien und FIFOs 0, die Prüfung „0 <= Grenze" bestünde, und danach
// liest `readAll()` unendlich weiter. Der wirksame Schutz ist `isFile()` —
// die Obergrenze fängt den zweiten Fall ab, die schlicht zu große reguläre Datei.
//
// ⚠️ Ein FIFO frisst dabei **keinen** Speicher, sondern blockiert für immer. Der
// Fehler sieht dann wie ein Hänger aus, nicht wie ein Leck — wer nur nach
// Speicherverbrauch sucht, findet ihn nie.
//
// ⚠️ Und: Ein Token schützt nicht davor. Wer sich anmeldet, kann trotzdem einen
// bösen Pfad schicken — und ein versehentlich falscher Pfad tut dasselbe wie ein
// böser.

#include <QByteArray>
#include <QString>

namespace qtmux::safefile {

/// Obergrenzen an EINER Stelle, damit sie begründet und auffindbar sind.
namespace limits {
/// Einstellungs-Export/-Import (JSON). Ein realer Export liegt bei wenigen KB.
inline constexpr qint64 kSettingsJson = 8 * 1024 * 1024;
/// Farbschema-Datei (iTerm/Xresources/Ghostty). Selbst iTerms XML bleibt winzig.
inline constexpr qint64 kColorScheme = 4 * 1024 * 1024;
/// Ein Scrollback-Dump je Pane. 10 000 Zeilen × 512 Spalten mit Farbcodes
/// bleiben deutlich darunter; alles darüber ist kein Verlauf mehr.
inline constexpr qint64 kHistoryDump = 64 * 1024 * 1024;
}  // namespace limits

struct Result {
    bool ok = false;
    QByteArray data;
    /// Übersetzter, für den Anwender lesbarer Grund. Leer, wenn `ok`.
    QString error;
};

/// Liest `path` **nur**, wenn es eine reguläre Datei ist und höchstens
/// `maxBytes` groß. Andernfalls kommt eine klare Fehlermeldung zurück, statt
/// dass still Speicher verbraucht wird.
///
/// Bewusst KEIN Verzeichnis-Gefängnis: Die Aufrufer hier bekommen ihren Pfad vom
/// Menschen über einen Dateidialog, und der darf überall hinzeigen. Wer eine
/// Route baut, deren Pfad aus dem Netz kommt, braucht zusätzlich eine
/// Einschränkung auf ein erlaubtes Verzeichnis — dann bitte hier vorbeiziehen
/// und diesen Absatz ändern.
[[nodiscard]] Result read(const QString &path, qint64 maxBytes);

}  // namespace qtmux::safefile

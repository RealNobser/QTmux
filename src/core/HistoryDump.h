#pragma once

// qtmux::historydump — Kopfzeile der gespeicherten Scrollback-Dumps (QTMUX-130).
//
// Der Dump selbst ist ein ANSI-Strom aus `VtScreen::serializeAnsi`. Weiche Umbrüche
// stehen darin bewusst OHNE CRLF, damit das Terminal beim Einspielen selbst auf die
// aktuelle Breite umbricht — der Dump trägt also KEINE Breite in sich, sondern erwartet
// eine. Genau daraus entstand der Fehler: Eingespielt wurde er in eine Session, die noch
// auf dem geratenen Startwert 80 stand, und `vterm_set_size` reflowt den Scrollback
// nicht mehr. Der falsche Umbruch war eingefroren.
//
// Im Normalfall löst das die Session, indem sie den Verlauf bis zur ersten echten
// Messung zurückhält (`Session::setPendingHistory`). Diese Kopfzeile deckt den Rest ab:
// ein Fenster, das seit dem Neustart nie sichtbar war, wird nie vermessen — dann ist die
// zuletzt bekannte Breite die beste verfügbare Näherung.
//
// 🔑 Die Kopfzeile ist OPTIONAL, und das ist der Kern des Entwurfs: Jede Datei aus einer
// älteren Fassung bleibt gültig und liefert einfach `0` (= unbekannt). Ein Pflicht-Header
// hätte jeden vorhandenen Verlauf auf einen Schlag entwertet — für eine Angabe, die nur
// in einem Randfall gebraucht wird.

#include <QByteArray>

namespace qtmux::historydump {

inline constexpr char kHeaderPrefix[] = "QTMUX-HISTORY 1 cols=";

/// Kopfzeile für einen Dump, der bei `cols` Spalten entstand. Leer bei `cols <= 0` —
/// eine unbekannte Breite wird nicht als Zahl behauptet.
inline QByteArray header(int cols) {
    if (cols <= 0) return {};
    return QByteArray(kHeaderPrefix) + QByteArray::number(cols) + '\n';
}

/// Trennt eine etwaige Kopfzeile ab: schneidet sie aus `data` heraus und liefert die
/// Breite. 0 = keine oder unlesbare Kopfzeile; `data` bleibt dann unangetastet.
///
/// ⚠️ Ohne Zeilenumbruch (abgeschnittene Datei) wird NICHT geschnitten — sonst
/// verschwände bei einem halb geschriebenen Dump der komplette Inhalt.
inline int takeHeader(QByteArray &data) {
    if (!data.startsWith(kHeaderPrefix)) return 0;
    const int nl = data.indexOf('\n');
    if (nl < 0) return 0;
    constexpr int prefixLen = int(sizeof(kHeaderPrefix)) - 1;
    bool ok = false;
    const int cols = data.mid(prefixLen, nl - prefixLen).toInt(&ok);
    data.remove(0, nl + 1);
    return ok && cols > 0 ? cols : 0;
}

}  // namespace qtmux::historydump

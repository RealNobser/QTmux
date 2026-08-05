#include "SafeFileRead.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>

namespace qtmux::safefile {

Result read(const QString &path, qint64 maxBytes)
{
    Result r;
    const QFileInfo fi(path);

    if (!fi.exists()) {
        r.error = QCoreApplication::translate(
            "safefile", "Die Datei „%1“ gibt es nicht.").arg(path);
        return r;
    }

    // 🔑 DIESE Prüfung ist der eigentliche Schutz, nicht die Größe darunter.
    // `/dev/zero`, `/dev/urandom`, `/proc/kcore` und FIFOs melden alle
    // `size() == 0` — die Größenprüfung allein ließe sie durch, und danach
    // liest `readAll()` unendlich (oder blockiert im Fall des FIFO für immer).
    if (!fi.isFile()) {
        r.error = QCoreApplication::translate(
            "safefile",
            "„%1“ ist keine gewöhnliche Datei. Geräte, Verzeichnisse und "
            "Warteschlangen werden nicht gelesen.").arg(path);
        return r;
    }

    if (fi.size() > maxBytes) {
        r.error = QCoreApplication::translate(
            "safefile", "Die Datei „%1“ ist zu groß (%2 MB, erlaubt sind %3 MB).")
            .arg(path)
            .arg(fi.size() / (1024.0 * 1024.0), 0, 'f', 1)
            .arg(maxBytes / (1024.0 * 1024.0), 0, 'f', 1);
        return r;
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        r.error = QCoreApplication::translate(
            "safefile", "Die Datei „%1“ lässt sich nicht öffnen: %2")
            .arg(path, f.errorString());
        return r;
    }

    // Trotz der Prüfungen oben gedeckelt lesen: Zwischen `stat` und `read` kann
    // die Datei getauscht worden sein, und `read(maxBytes + 1)` kostet nichts.
    r.data = f.read(maxBytes + 1);
    f.close();

    if (r.data.size() > maxBytes) {
        r.data.clear();
        r.error = QCoreApplication::translate(
            "safefile",
            "Die Datei „%1“ ist beim Lesen über die erlaubte Größe gewachsen.").arg(path);
        return r;
    }

    r.ok = true;
    return r;
}

}  // namespace qtmux::safefile

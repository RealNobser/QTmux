#pragma once

#include <QString>

namespace qtmux {

/// Aktueller Git-Zustand eines Arbeitsverzeichnisses (QTMUX-58).
///
/// Hintergrund: In der Sidebar laufen oft mehrere Agenten im **selben** Repository —
/// Titel und Verzeichnis sind dann identisch, der Branch ist das einzige
/// Unterscheidungsmerkmal.
struct GitInfo {
    QString branch;             ///< z. B. "main" oder "feat/qtmux-58-git-branch"; leer bei detached HEAD
    bool    detached = false;   ///< true = HEAD zeigt direkt auf einen Commit, nicht auf einen Branch
    QString shortSha;           ///< nur bei `detached` gefüllt (7 Zeichen), sonst leer — s. Hinweis unten
    bool    valid    = false;   ///< false = kein Repository, unlesbar oder unverständliches HEAD

    /// Ermittelt den Git-Zustand für `dir`, indem die Dateien `.git/HEAD` **gelesen**
    /// werden — es wird **kein** git-Prozess gestartet und keine Bibliothek eingebunden.
    ///
    /// Das ist bewusst so: Der Aufruf erfolgt später zyklisch (Polling je Session,
    /// s. `SessionModel`). Ein `git`-Aufruf je Session und Intervall wäre ein Prozessstart
    /// pro Sekunde und Kachel; hier bleiben es ein bis zwei kleine Dateilesungen.
    ///
    /// Behandelte Fälle (jeder ist ein eigener Test in `tst_gitinfo`):
    /// - `dir` liegt **tief** im Repo → es wird nach oben gesucht, bis `.git` auftaucht
    ///   oder die Dateisystemwurzel erreicht ist.
    /// - `.git` ist **keine Verzeichnis, sondern eine Datei** — so sieht es in einem
    ///   `git worktree` und in einem Submodul aus. Inhalt: `gitdir: <pfad>`, wobei der
    ///   Pfad **relativ** sein darf (bei Submodulen der Normalfall) und dann gegen das
    ///   Verzeichnis der `.git`-Datei aufgelöst wird.
    /// - **Detached HEAD**: `HEAD` enthält dann einen rohen SHA statt `ref: refs/heads/…`.
    /// - **bare-Repository** (`<name>.git`, kein Arbeitsbaum): `HEAD`, `objects/` und `refs/`
    ///   liegen direkt im Verzeichnis, ein `.git` gibt es nicht. Erkannt wird an allen drei
    ///   Marken zugleich — eine beliebige Datei namens `HEAD` allein darf kein Repo vortäuschen.
    /// - Kein Repo, nicht existierendes oder unlesbares Verzeichnis, kaputtes `HEAD`
    ///   → `valid == false`, ohne Absturz und **ohne Logausgabe** (der Normalfall „hier ist
    ///   einfach kein Repo" darf die Konsole nicht mit Warnungen fluten).
    ///
    /// ⚠️ `shortSha` bleibt bei einem Branch bewusst leer: Die Commit-ID stünde dann in
    /// `refs/heads/<name>` — oder, wenn git gepackt hat, gar nicht mehr als Datei, sondern
    /// nur in `packed-refs`. Beides zu lesen wäre Mehraufwand für eine Angabe, die die
    /// Sidebar nicht braucht.
    static GitInfo forDirectory(const QString &dir);
};

} // namespace qtmux

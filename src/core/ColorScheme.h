#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QList>

namespace qtmux {

/// Ein Terminal-Farbschema: Default-Vorder-/Hintergrund, Cursor und die 16
/// ANSI-Farben (0–7 normal, 8–15 hell). Farben als 0xRRGGBB (Gui-frei → kein QColor).
struct ColorScheme {
    QString name;
    bool dark = true;            // dunkles Schema? (nur informativ, z. B. für Vorschau)
    bool builtin = true;         // eingebaut (nicht löschbar/überschreibbar)
    quint32 fg = 0xe6e7ee;       // Default-Vordergrund
    quint32 bg = 0x1e1f29;       // Default-Hintergrund
    quint32 cursor = 0xe6e7ee;   // Cursor-Farbe
    quint32 ansi[16] = {0};      // 16 ANSI-Palettenfarben
};

/// Prozessweite Verwaltung der Terminal-Farbschemata (eingebaut + importiert).
/// Gui-frei (nur Qt Core) — von `Session`/`VtScreen` (Palette setzen) UND von QML
/// (Auswahl/Import via in main.cpp registriertem Singleton `ColorSchemes`) genutzt.
/// Das aktuelle Schema und importierte Schemata werden via QSettings persistiert.
class ColorSchemeRegistry : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString current READ current WRITE setCurrent NOTIFY changed)
    Q_PROPERTY(QStringList names READ names NOTIFY listChanged)
public:
    /// Prozessweite Instanz (geteilt zwischen Core und QML).
    static ColorSchemeRegistry *instance();

    QString current() const { return m_current; }
    void setCurrent(const QString &name);     // wechselt + persistiert + emittiert changed()
    QStringList names() const;

    /// Schema nach Name (Fallback: aktuelles bzw. erstes, nie ungültig).
    const ColorScheme &scheme(const QString &name) const;
    const ColorScheme &currentScheme() const { return scheme(m_current); }

    /// Farben eines Schemas als Map für die QML-Vorschau:
    /// { fg, bg, cursor: "#rrggbb", dark: bool, ansi: ["#rrggbb", …] }.
    Q_INVOKABLE QVariantMap colors(const QString &name) const;

    /// Importiert ein Schema aus einer Datei (iTerm `.itermcolors`, Xresources oder
    /// Ghostty-Config). Gibt den Namen des importierten Schemas zurück (leer bei
    /// Fehler), selektiert es und persistiert.
    Q_INVOKABLE QString importFile(const QString &path);

signals:
    void changed();       // aktuelles Schema gewechselt (oder dessen Farben)
    void listChanged();   // Schema-Liste geändert (Import)

private:
    explicit ColorSchemeRegistry(QObject *parent = nullptr);
    void loadBuiltins();
    void loadPersisted();
    void persist() const;
    int indexOf(const QString &name) const;

    QList<ColorScheme> m_schemes;
    QString m_current;
};

} // namespace qtmux

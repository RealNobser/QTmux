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
    // Je ein Schema für den Hell- und den Dunkel-Modus; das aktive richtet sich nach
    // dem effektiven Modus (von Theme via setDark gesetzt) und färbt die GANZE App.
    Q_PROPERTY(QString darkScheme READ darkScheme WRITE setDarkScheme NOTIFY selectionChanged)
    Q_PROPERTY(QString lightScheme READ lightScheme WRITE setLightScheme NOTIFY selectionChanged)
    Q_PROPERTY(QString current READ current NOTIFY changed)   // aktiver Schema-Name (read-only)
    Q_PROPERTY(QStringList names READ names NOTIFY listChanged)
public:
    /// Prozessweite Instanz (geteilt zwischen Core und QML).
    static ColorSchemeRegistry *instance();

    QString darkScheme() const { return m_darkChoice; }
    QString lightScheme() const { return m_lightChoice; }
    void setDarkScheme(const QString &name);   // Auswahl für den Dunkel-Modus
    void setLightScheme(const QString &name);  // Auswahl für den Hell-Modus
    QString current() const { return activeName(); }   // aktiver Name je nach Modus
    QStringList names() const;

    /// Effektiven Modus setzen (von Theme aus mode/OS abgeleitet). Wechselt dadurch das
    /// aktive Schema, wird changed() emittiert (Session + Theme zeichnen neu).
    void setDark(bool dark);
    bool isDark() const { return m_dark; }

    /// Schema nach Name (Fallback: aktuelles bzw. erstes, nie ungültig).
    const ColorScheme &scheme(const QString &name) const;
    const ColorScheme &currentScheme() const { return scheme(activeName()); }

    /// Farben eines Schemas als Map für die QML-Vorschau:
    /// { fg, bg, cursor: "#rrggbb", dark: bool, ansi: ["#rrggbb", …] }.
    Q_INVOKABLE QVariantMap colors(const QString &name) const;

    /// Importiert ein Schema aus einer Datei (iTerm `.itermcolors`, Xresources oder
    /// Ghostty-Config). Gibt den Namen des importierten Schemas zurück (leer bei
    /// Fehler), selektiert es und persistiert.
    Q_INVOKABLE QString importFile(const QString &path);

signals:
    void changed();          // aktives Schema gewechselt (Modus oder Auswahl) → neu zeichnen
    void selectionChanged(); // Hell-/Dunkel-Auswahl geändert (für Combo-Bindings)
    void listChanged();      // Schema-Liste geändert (Import)

private:
    explicit ColorSchemeRegistry(QObject *parent = nullptr);
    void loadBuiltins();
    void loadPersisted();
    void persist() const;
    int indexOf(const QString &name) const;
    QString activeName() const { return m_dark ? m_darkChoice : m_lightChoice; }

    QList<ColorScheme> m_schemes;
    QString m_darkChoice;    // gewähltes Schema im Dunkel-Modus
    QString m_lightChoice;   // gewähltes Schema im Hell-Modus
    bool m_dark = true;      // effektiver Modus (von Theme gesetzt)
};

} // namespace qtmux

#pragma once

#include <QObject>
#include <QColor>
#include <qqmlintegration.h>

namespace qtmux {

/// Zentrale Farbpalette mit Dark-/Light-/System-Modus, als QML-Singleton (`Theme.*`).
/// `mode` (System/Light/Dark) wird via QSettings persistiert. Bei System folgt die
/// Palette dem Betriebssystem (QStyleHints::colorScheme) und reagiert live auf Wechsel.
/// Alle Farb-Properties teilen sich das NOTIFY-Signal `changed`.
class Theme : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
public:
    enum Mode { System, Light, Dark };
    Q_ENUM(Mode)

private:
    Q_PROPERTY(Mode mode READ mode WRITE setMode NOTIFY changed)
    Q_PROPERTY(bool dark READ dark NOTIFY changed)  // effektiver Modus (App)
    // Reines OS-Farbschema (unabhängig vom App-Modus). Native macOS-Menüs
    // folgen IMMER dem System; ihre Icon-Tönung muss daher hieran hängen.
    Q_PROPERTY(bool systemDark READ systemDark NOTIFY changed)
    Q_PROPERTY(QColor menuIcon READ menuIcon NOTIFY changed)
    Q_PROPERTY(QColor bgSidebar       READ bgSidebar       NOTIFY changed)
    Q_PROPERTY(QColor bgMain          READ bgMain          NOTIFY changed)
    Q_PROPERTY(QColor bgElevated      READ bgElevated      NOTIFY changed)
    Q_PROPERTY(QColor sidebarHover    READ sidebarHover    NOTIFY changed)
    Q_PROPERTY(QColor sidebarSelected READ sidebarSelected NOTIFY changed)
    Q_PROPERTY(QColor border          READ border          NOTIFY changed)
    Q_PROPERTY(QColor accent          READ accent          NOTIFY changed)
    Q_PROPERTY(QColor accentText      READ accentText      NOTIFY changed)
    Q_PROPERTY(QColor textBright      READ textBright      NOTIFY changed)
    Q_PROPERTY(QColor textDim         READ textDim         NOTIFY changed)
    Q_PROPERTY(QColor terminalBg      READ terminalBg      NOTIFY changed)
    Q_PROPERTY(QColor terminalFg      READ terminalFg      NOTIFY changed)
    Q_PROPERTY(QColor terminalCursor  READ terminalCursor  NOTIFY changed)
    // Statusrollen (QTMUX-142) — Namen nach dem Hub-KitTheme (ok/warn/danger), dazu
    // `muted` für „geschlossen". Je ein fester Hell- und Dunkel-Wert; welcher gilt,
    // entscheidet die Helligkeit der echten Hauptfläche, nicht der App-Modus.
    Q_PROPERTY(QColor ok              READ ok              NOTIFY changed)
    Q_PROPERTY(QColor warn            READ warn            NOTIFY changed)
    Q_PROPERTY(QColor danger          READ danger          NOTIFY changed)
    Q_PROPERTY(QColor muted           READ muted           NOTIFY changed)
public:
    explicit Theme(QObject *parent = nullptr);

    Mode mode() const { return m_mode; }
    void setMode(Mode mode);
    bool dark() const;                 // löst System zu konkretem Hell/Dunkel auf
    bool systemDark() const;           // reines OS-Schema (für native Menüs)
    QColor menuIcon() const;           // Icon-Tönung für native Menüs (folgt System)
    Q_INVOKABLE void toggle();         // schaltet explizit Hell<->Dunkel (Ctrl+D)
    /// Liest `ui/themeMode` erneut (nach Reset/Import, Stufe 6) — bewusst OHNE zu
    /// persistieren, sonst schriebe ein Zurücksetzen den Standardwert gleich wieder in
    /// die Einstellungen und der Schlüssel wäre nach dem Reset sofort wieder da.
    Q_INVOKABLE void reload();

    QColor bgSidebar() const;
    QColor bgMain() const;
    QColor bgElevated() const;
    QColor sidebarHover() const;
    QColor sidebarSelected() const;
    QColor border() const;
    QColor accent() const;
    QColor accentText() const;         // lesbare Schrift auf accent (hell/dunkel je Luminanz)
    QColor textBright() const;
    QColor textDim() const;
    QColor terminalBg() const;
    QColor terminalFg() const;
    QColor terminalCursor() const;

    // Statusrollen. Vertrag (geprüft in tst_statuscolors, beide Standardschemata):
    // als Punkt/Rand/Fläche >= 3:1 gegen bgSidebar, sidebarHover, sidebarSelected und
    // bgElevated; warn zusätzlich als TEXT >= 4,5:1 gegen bgMain, bgSidebar,
    // sidebarHover und bgElevated, danger als TEXT >= 4,5:1 gegen bgElevated.
    // ok/muted sind nur als Punkt geprüft — wer sie als Text einsetzt, erweitert
    // zuerst den Vertrag im Test.
    QColor ok() const;
    QColor warn() const;
    QColor danger() const;
    QColor muted() const;
    /// true = Dunkel-Werte der Statusrollen (Hauptfläche dunkel, Rec.-601-Luma < 0,5).
    bool statusDark() const;

signals:
    void changed();

private:
    Mode m_mode = System;
};

} // namespace qtmux

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
    Q_PROPERTY(QColor textBright      READ textBright      NOTIFY changed)
    Q_PROPERTY(QColor textDim         READ textDim         NOTIFY changed)
    Q_PROPERTY(QColor terminalBg      READ terminalBg      NOTIFY changed)
    Q_PROPERTY(QColor terminalFg      READ terminalFg      NOTIFY changed)
public:
    explicit Theme(QObject *parent = nullptr);

    Mode mode() const { return m_mode; }
    void setMode(Mode mode);
    bool dark() const;                 // löst System zu konkretem Hell/Dunkel auf
    bool systemDark() const;           // reines OS-Schema (für native Menüs)
    QColor menuIcon() const;           // Icon-Tönung für native Menüs (folgt System)
    Q_INVOKABLE void toggle();         // schaltet explizit Hell<->Dunkel (Ctrl+D)

    QColor bgSidebar() const;
    QColor bgMain() const;
    QColor bgElevated() const;
    QColor sidebarHover() const;
    QColor sidebarSelected() const;
    QColor border() const;
    QColor accent() const;
    QColor textBright() const;
    QColor textDim() const;
    QColor terminalBg() const;
    QColor terminalFg() const;

signals:
    void changed();

private:
    Mode m_mode = System;
};

} // namespace qtmux

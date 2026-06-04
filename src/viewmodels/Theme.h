#pragma once

#include <QObject>
#include <QColor>
#include <qqmlintegration.h>

namespace qtmux {

/// Zentrale Farbpalette mit Dark-/Light-Modus, als QML-Singleton verfügbar (`Theme.*`).
/// Der Modus wird via QSettings persistiert. Alle Farb-Properties teilen sich das
/// NOTIFY-Signal `changed`, sodass QML-Bindings beim Umschalten sofort aktualisieren.
class Theme : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY changed)
    Q_PROPERTY(QColor bgSidebar       READ bgSidebar       NOTIFY changed)
    Q_PROPERTY(QColor bgMain          READ bgMain          NOTIFY changed)
    Q_PROPERTY(QColor bgElevated      READ bgElevated      NOTIFY changed)
    Q_PROPERTY(QColor sidebarHover    READ sidebarHover    NOTIFY changed)
    Q_PROPERTY(QColor sidebarSelected READ sidebarSelected NOTIFY changed)
    Q_PROPERTY(QColor border          READ border          NOTIFY changed)
    Q_PROPERTY(QColor accent          READ accent          NOTIFY changed)
    Q_PROPERTY(QColor textBright      READ textBright      NOTIFY changed)
    Q_PROPERTY(QColor textDim         READ textDim         NOTIFY changed)
public:
    explicit Theme(QObject *parent = nullptr);

    bool dark() const { return m_dark; }
    void setDark(bool dark);
    Q_INVOKABLE void toggle() { setDark(!m_dark); }

    QColor bgSidebar() const;
    QColor bgMain() const;
    QColor bgElevated() const;
    QColor sidebarHover() const;
    QColor sidebarSelected() const;
    QColor border() const;
    QColor accent() const;
    QColor textBright() const;
    QColor textDim() const;

signals:
    void changed();

private:
    bool m_dark = true;
};

} // namespace qtmux

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QMap>
#include <QList>
#include <QPair>

namespace qtmux {

/// Verwaltung der konfigurierbaren Tastenkürzel (QTMUX-15). Gui-frei (nur Qt Core):
/// hält die Standard-Sequenzen je Aktions-ID plus benutzerdefinierte Overrides und
/// persistiert nur die Overrides via QSettings. Multi-Chord wird unterstützt, indem
/// eine Sequenz aus mehreren kommagetrennten Akkorden bestehen darf (QKeySequence-
/// Format, z. B. "Ctrl+K, Ctrl+S") — das Bilden des Strings aus einem Tasten-Event
/// macht die Gui-Seite (AppController::keyChord), damit hier kein QtGui nötig ist.
///
/// QML bindet die Action.shortcut an `Hotkeys.bindings[id]` (QVariantMap mit NOTIFY),
/// sodass eine Neubelegung sofort überall greift.
class HotkeyRegistry : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap bindings READ bindings NOTIFY changed)
public:
    /// Prozessweite Instanz (in main.cpp als Context-Property `Hotkeys` registriert).
    static HotkeyRegistry *instance();

    /// Effektive Sequenzen (Default, überlagert vom Override) als id->Sequenz-Map.
    QVariantMap bindings() const;

    /// Aktions-IDs in UI-Reihenfolge.
    Q_INVOKABLE QStringList actionIds() const;
    /// Effektive Sequenz einer Aktion (Override falls vorhanden, sonst Default).
    Q_INVOKABLE QString sequence(const QString &id) const;
    /// Werkseinstellung der Aktion.
    Q_INVOKABLE QString defaultSequence(const QString &id) const;
    /// True, wenn die Aktion vom Default abweicht (benutzerdefiniert).
    Q_INVOKABLE bool isCustom(const QString &id) const;

    /// Belegt die Aktion neu. Leere Sequenz oder Gleichheit mit dem Default entfernt
    /// den Override (= zurück zur Werkseinstellung). Persistiert + meldet `changed`.
    Q_INVOKABLE void setBinding(const QString &id, const QString &seq);
    /// Setzt eine Aktion auf ihren Default zurück.
    Q_INVOKABLE void reset(const QString &id);
    /// Setzt alle Aktionen zurück.
    Q_INVOKABLE void resetAll();

    /// Liefert die Aktions-ID, die `seq` bereits belegt (außer `exceptId`), sonst "".
    /// Vergleich case-insensitiv über das kanonische Portable-Format.
    Q_INVOKABLE QString conflict(const QString &seq, const QString &exceptId) const;

signals:
    void changed();

private:
    explicit HotkeyRegistry(QObject *parent = nullptr);
    void loadOverrides();
    void persist() const;
    int defaultIndex(const QString &id) const;

    QList<QPair<QString, QString>> m_defaults;  // id -> Default-Sequenz (Reihenfolge = UI)
    QMap<QString, QString> m_overrides;         // id -> benutzerdefinierte Sequenz
};

} // namespace qtmux

#pragma once

#include <QObject>

namespace qtmux {

/// Systemweiter (globaler) Hotkey für den Quake-Modus: blendet das Fenster auch
/// dann ein/aus, wenn QTmux nicht im Vordergrund ist. Plattform-spezifisch:
/// macOS via Carbon `RegisterEventHotKey` (funktioniert ohne Bedienungshilfen-Rechte),
/// Windows/Linux vorerst Stub (Feature dort deaktiviert). Default-Tastenkombi: Ctrl+`.
class GlobalHotkey : public QObject {
    Q_OBJECT
public:
    explicit GlobalHotkey(QObject *parent = nullptr);
    ~GlobalHotkey() override;

    bool isEnabled() const { return m_enabled; }
    /// Hotkey (de)registrieren. Gibt true zurück, wenn der Zielzustand erreicht wurde
    /// (auf nicht unterstützten Plattformen liefert true nur für `false`).
    Q_INVOKABLE bool setEnabled(bool on);

signals:
    /// Der globale Hotkey wurde gedrückt (im GUI-Thread zugestellt).
    void activated();

private:
    bool m_enabled = false;
};

} // namespace qtmux

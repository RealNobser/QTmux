#pragma once

// Bildziel `qtmux-bilder` (CMake-Schalter QTMUX_UPDATE_PICTURES, Vorgabe an):
// Bilder der Update-Oberfläche von QTmux in jedem Zustand des Ablaufs.
//
// 🔑 Diese Datei und der Szenen-Baustein des Hubs (appupdate::UpdateShowcase) werden
// NUR in dieses Programm übersetzt. Die App `qtmux` enthält beide nicht und kennt
// auch den Schalter `--update-picture` nicht; das misst der Wächter
// test_release_has_no_showcase am gebauten Programm. `qtmux-bilder` liegt in keinem
// Paket (DMG, MSI, ZIP, AppImage nehmen nur `qtmux`).
//
// Aufruf (nur zusammen mit dem Selbsttest-Ordner, der Einstellungen und Profil vom
// Anwender trennt):
//   qtmux-bilder --selftest-update <ordner> --update-picture <szene>
//                --screenshot <png> [--picture-theme light|dark]
//                [--picture-language de|en]
//
// Szenen: alle des Hubs (UpdateShowcase::scenes()), dazu
//   <hint-…>-narrow        derselbe Hinweis bei 720 px Fensterbreite
//   qtmux-other-instance   „von Hand“, weil eine zweite QTmux-Instanz aus demselben
//                          Ort läuft (QTmux erlaubt mehrere Instanzen)
//   qtmux-settings-port    Einstellungen › Updates, manueller Proxy mit einem Port
//                          außerhalb 0…65535 (das Feld ist markiert)
//
// Der Szenen-Baustein bringt einen ECHTEN appupdate::UpdateFlow mit eigenen
// Einstellungen, einem zur Laufzeit signierten Baum (Wegwerf-Schlüssel), einem
// Server auf 127.0.0.1 und einem Installationsweg, der nie etwas startet. Die
// Bridge zeigt diesen Ablauf statt eines eigenen (UpdateBridge::Wiring::externalFlow).

#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

namespace appupdate {
class UpdateFlow;
class UpdateShowcase;
} // namespace appupdate

namespace qtmux {

class UpdatePicture {
public:
    /// `installed` ist die reine Version (ohne Build-Suffix), `buildSuffix` z. B.
    /// "+4db7212"; beide erscheinen auf der Seite „Updates“.
    UpdatePicture(const QString &scene, const QString &language, const QString &installed,
                  const QString &buildSuffix);
    ~UpdatePicture();

    [[nodiscard]] static QStringList scenes();

    /// Fährt den Ablauf in die Szene; `reached(ok)` kommt einmal, sobald er dort
    /// steht. false für eine unbekannte Szene (Liste im Log).
    bool begin(std::function<void(bool ok)> reached);

    /// Der Ablauf der Szene (gültig nach begin()).
    [[nodiscard]] appupdate::UpdateFlow *flow() const;
    /// "dialog" | "hint" | "settings" | "proxy" — was fotografiert wird.
    [[nodiscard]] QString surface() const { return m_surface; }
    /// Fensterbreite in px; 0 = Vorgabe des Hauptfensters.
    [[nodiscard]] int windowWidth() const { return m_width; }
    /// Nicht leer: dieser Text kommt vor dem Bild ins Portfeld der Seite.
    [[nodiscard]] QString portText() const { return m_portText; }

private:
    QString m_scene;
    QString m_hubScene;
    QString m_surface;
    int m_width = 0;
    QString m_portText;
    bool m_otherInstance = false;
    std::unique_ptr<appupdate::UpdateShowcase> m_showcase;
};

} // namespace qtmux

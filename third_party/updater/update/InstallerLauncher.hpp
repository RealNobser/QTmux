#pragma once

// appupdate — launching the downloaded installer, per artifact kind.
//
//   msi      → msiexec /i <path>          (WiX MajorUpgrade handles the rest)
//   dmg      → open <path>                (macOS mounts and shows the volume)
//   appimage → self-replace via $APPIMAGE (.new → rename, .old fallback)
//
// installerLaunchPlan() is the pure, testable half: it only computes what
// WOULD run. launchInstaller() executes the plan (QProcess::startDetached
// or the AppImage rename dance). Apps that want a dry-run mode log the
// plan's description instead of calling launchInstaller().

#include <QString>
#include <QStringList>

#include <optional>

namespace appupdate {

struct LaunchPlan {
    QString program;        // empty for the AppImage self-replace path
    QStringList arguments;
    bool selfReplace = false;
    QString description;    // human-readable, for dry-run logging
};

// Computes the launch plan for `localPath` with manifest `kind`
// ("msi" | "dmg" | "appimage"). `appImagePath` is the running AppImage
// ($APPIMAGE) and only matters for kind "appimage" — empty means "not
// running from an AppImage", which yields nullopt (nothing to replace).
// Unknown kinds yield nullopt.
[[nodiscard]] std::optional<LaunchPlan> installerLaunchPlan(
    const QString& localPath, const QString& kind,
    const QString& appImagePath);

// Executes the plan. Returns false and fills errorOut on failure. The
// AppImage path replaces the running image: copy to <target>.new, make it
// executable, move the current image to <target>.old (kept as fallback),
// rename .new into place. The new binary runs on the NEXT start — the
// running process stays untouched.
bool launchInstaller(const QString& localPath, const QString& kind,
                     QString* errorOut = nullptr);

// The AppImage self-replace, exposed for tests (works on any two files).
bool replaceAppImage(const QString& newImagePath, const QString& appImagePath,
                     QString* errorOut = nullptr);

}  // namespace appupdate

#include "update/InstallerLauncher.hpp"

#include <QFile>
#include <QFileInfo>
#include <QProcess>

namespace appupdate {

std::optional<LaunchPlan> installerLaunchPlan(const QString& localPath,
                                              const QString& kind,
                                              const QString& appImagePath)
{
    const QString k = kind.toLower();
    LaunchPlan plan;
    if (k == QLatin1String("msi")) {
        plan.program = QStringLiteral("msiexec");
        plan.arguments = {QStringLiteral("/i"), localPath};
        plan.description =
            QStringLiteral("msiexec /i \"%1\"").arg(localPath);
        return plan;
    }
    if (k == QLatin1String("dmg")) {
        plan.program = QStringLiteral("open");
        plan.arguments = {localPath};
        plan.description = QStringLiteral("open \"%1\"").arg(localPath);
        return plan;
    }
    if (k == QLatin1String("appimage")) {
        if (appImagePath.isEmpty()) {
            return std::nullopt;  // not running from an AppImage
        }
        plan.selfReplace = true;
        plan.description =
            QStringLiteral("replace AppImage \"%1\" with \"%2\" "
                           "(.old kept as fallback)")
                .arg(appImagePath, localPath);
        return plan;
    }
    return std::nullopt;
}

bool replaceAppImage(const QString& newImagePath, const QString& appImagePath,
                     QString* errorOut)
{
    const auto fail = [errorOut](const QString& why) {
        if (errorOut) *errorOut = why;
        return false;
    };

    if (!QFile::exists(newImagePath)) {
        return fail(QStringLiteral("new image %1 does not exist").arg(newImagePath));
    }
    if (!QFile::exists(appImagePath)) {
        return fail(QStringLiteral("target %1 does not exist").arg(appImagePath));
    }

    const QString tmpPath = appImagePath + QStringLiteral(".new");
    const QString oldPath = appImagePath + QStringLiteral(".old");

    QFile::remove(tmpPath);
    if (!QFile::copy(newImagePath, tmpPath)) {
        return fail(QStringLiteral("cannot copy to %1").arg(tmpPath));
    }
    QFile tmp(tmpPath);
    tmp.setPermissions(tmp.permissions() | QFileDevice::ExeOwner |
                       QFileDevice::ExeGroup | QFileDevice::ExeOther);

    QFile::remove(oldPath);
    if (!QFile::rename(appImagePath, oldPath)) {
        QFile::remove(tmpPath);
        return fail(QStringLiteral("cannot move current image aside"));
    }
    if (!QFile::rename(tmpPath, appImagePath)) {
        // Roll the old image back into place — never leave the user with
        // no runnable binary at $APPIMAGE.
        QFile::rename(oldPath, appImagePath);
        QFile::remove(tmpPath);
        return fail(QStringLiteral("cannot move new image into place "
                                   "(old image restored)"));
    }
    return true;
}

bool launchInstaller(const QString& localPath, const QString& kind,
                     QString* errorOut)
{
    const QString appImage =
        QString::fromLocal8Bit(qgetenv("APPIMAGE"));
    const auto plan = installerLaunchPlan(localPath, kind, appImage);
    if (!plan) {
        if (errorOut) {
            *errorOut = QStringLiteral("no launch plan for kind '%1'").arg(kind);
        }
        return false;
    }
    if (plan->selfReplace) {
        return replaceAppImage(localPath, appImage, errorOut);
    }
    if (!QProcess::startDetached(plan->program, plan->arguments)) {
        if (errorOut) {
            *errorOut = QStringLiteral("failed to start %1").arg(plan->program);
        }
        return false;
    }
    return true;
}

}  // namespace appupdate

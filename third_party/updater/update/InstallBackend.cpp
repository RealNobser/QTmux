#include "update/InstallBackend.hpp"

#include "update/ErrorText.hpp"
#include "update/InstallerLauncher.hpp"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRandomGenerator>
#include <QSet>
#include <QTimer>

namespace appupdate {

// ---- Names ----------------------------------------------------------------

QString installOutcomeResultName(InstallOutcome::Result r)
{
    switch (r) {
    case InstallOutcome::Result::Ok:         return QStringLiteral("ok");
    case InstallOutcome::Result::Failed:     return QStringLiteral("failed");
    case InstallOutcome::Result::RolledBack: return QStringLiteral("rolled-back");
    case InstallOutcome::Result::Cancelled:  return QStringLiteral("cancelled");
    }
    return QStringLiteral("failed");
}

QString manualReasonName(ManualReason r)
{
    switch (r) {
    case ManualReason::None:                 return QStringLiteral("none");
    case ManualReason::UnsupportedKind:      return QStringLiteral("unsupported-kind");
    case ManualReason::NotAnAppImage:        return QStringLiteral("not-an-appimage");
    case ManualReason::LocationNotWritable:  return QStringLiteral("location-not-writable");
    case ManualReason::RunningFromDiskImage: return QStringLiteral("running-from-disk-image");
    case ManualReason::AppTranslocated:      return QStringLiteral("app-translocated");
    case ManualReason::NotFamilySigned:      return QStringLiteral("not-family-signed");
    case ManualReason::HelperUnavailable:    return QStringLiteral("helper-unavailable");
    case ManualReason::Quarantined:          return QStringLiteral("quarantined");
    case ManualReason::NotInstalledPackage:  return QStringLiteral("not-installed-package");
    case ManualReason::OtherInstanceRunning: return QStringLiteral("other-instance-running");
    case ManualReason::AppManagementDenied:  return QStringLiteral("app-management-denied");
    case ManualReason::LocationShared:       return QStringLiteral("location-shared");
    case ManualReason::NotMeasuredOnThisPlatform:
        return QStringLiteral("not-measured-on-this-platform");
    case ManualReason::FileSystemUnsupported: return QStringLiteral("file-system-unsupported");
    case ManualReason::Other:                return QStringLiteral("other");
    }
    return QStringLiteral("other");
}

QString installMethodName(InstallMethod m)
{
    switch (m) {
    case InstallMethod::Automatic:       return QStringLiteral("automatic");
    case InstallMethod::SystemInstaller: return QStringLiteral("system-installer");
    case InstallMethod::Manual:          return QStringLiteral("manual");
    }
    return QStringLiteral("manual");
}

// ---- Attempt mark -----------------------------------------------------------

QString newInstallAttemptId()
{
    QByteArray raw(kInstallAttemptIdChars / 2, Qt::Uninitialized);
    QRandomGenerator::system()->generate(raw.begin(), raw.end());
    return QString::fromLatin1(raw.toHex());
}

// ---- Start confirmation ---------------------------------------------------

namespace {
QString oneLineValue(QString s)
{
    s.replace(QLatin1Char('\r'), QLatin1Char(' '));
    s.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return s;
}
}  // namespace

bool writeStartConfirmation(const QString& path, const StartConfirmation& c,
                            QString* errorOut)
{
    const auto fail = [errorOut](const QString& why) {
        if (errorOut) *errorOut = why;
        return false;
    };
    if (path.isEmpty()) return fail(QStringLiteral("no confirmation path"));
    if (c.version.trimmed().isEmpty()) return fail(QStringLiteral("no version to confirm"));
    const QString tmp = path + QStringLiteral(".tmp");
    QFile::remove(tmp);
    QFile f(tmp);
    if (!f.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        return fail(QStringLiteral("cannot create the confirmation file: %1").arg(fileErrorName(f.error())));
    }
    QString text = QStringLiteral("version=%1\n").arg(oneLineValue(c.version));
    if (!c.attempt.isEmpty()) text += QStringLiteral("attempt=%1\n").arg(oneLineValue(c.attempt));
    if (c.pid > 0) text += QStringLiteral("pid=%1\n").arg(c.pid);
    const QByteArray bytes = text.toUtf8();
    if (f.write(bytes) != bytes.size() || !f.flush()) {
        f.close();
        QFile::remove(tmp);
        return fail(QStringLiteral("cannot write %1").arg(tmp));
    }
    f.close();
    QFile::remove(path);
    if (!QFile::rename(tmp, path)) {
        QFile::remove(tmp);
        return fail(QStringLiteral("cannot rename to %1").arg(path));
    }
    if (errorOut) errorOut->clear();
    return true;
}

std::optional<StartConfirmation> parseStartConfirmation(const QByteArray& bytes,
                                                        QString* errorOut)
{
    const auto fail = [errorOut](const QString& why) -> std::optional<StartConfirmation> {
        if (errorOut) *errorOut = why;
        return std::nullopt;
    };
    if (bytes.size() > kInstallOutcomeMaxBytes) {
        return fail(QStringLiteral("confirmation larger than %1 bytes").arg(kInstallOutcomeMaxBytes));
    }
    StartConfirmation out;
    QSet<QString> seen;
    const QStringList lines = QString::fromUtf8(bytes).split(QLatin1Char('\n'));
    for (QString line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) line.chop(1);
        const qsizetype eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0) continue;
        const QString key = line.left(eq).trimmed();
        const QString value = line.mid(eq + 1).trimmed();
        if (seen.contains(key)) {
            return fail(QStringLiteral("key '%1' occurs more than once").arg(key.left(40)));
        }
        seen.insert(key);
        if (key == QLatin1String("version")) {
            out.version = value;
        } else if (key == QLatin1String("attempt")) {
            out.attempt = value;
        } else if (key == QLatin1String("pid")) {
            bool ok = false;
            const qint64 pid = value.toLongLong(&ok);
            if (!ok || pid <= 0) return fail(QStringLiteral("pid '%1' is no positive number").arg(value.left(40)));
            out.pid = pid;
        }
    }
    if (out.version.isEmpty()) return fail(QStringLiteral("confirmation has no 'version' line"));
    if (errorOut) errorOut->clear();
    return out;
}

// ---- Result file ----------------------------------------------------------

std::optional<InstallOutcome> parseInstallOutcome(const QByteArray& bytes,
                                                  QString* errorOut)
{
    const auto fail = [errorOut](const QString& why) -> std::optional<InstallOutcome> {
        if (errorOut) *errorOut = why;
        return std::nullopt;
    };
    if (bytes.size() > kInstallOutcomeMaxBytes) {
        return fail(QStringLiteral("result file larger than %1 bytes")
                        .arg(kInstallOutcomeMaxBytes));
    }

    InstallOutcome out;
    bool haveResult = false;
    QSet<QString> seen;  // a key twice makes the file unreadable
    const QString text = QString::fromUtf8(bytes);
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (QString line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) line.chop(1);
        const qsizetype eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0) continue;  // empty line, comment-ish, no key: ignored
        const QString key = line.left(eq).trimmed();
        const QString value = line.mid(eq + 1);
        if (seen.contains(key)) {
            return fail(QStringLiteral("key '%1' occurs more than once").arg(key.left(40)));
        }
        seen.insert(key);
        if (key == QLatin1String("result")) {
            const QString v = value.trimmed();
            if (v == QLatin1String("ok")) out.result = InstallOutcome::Result::Ok;
            else if (v == QLatin1String("failed")) out.result = InstallOutcome::Result::Failed;
            else if (v == QLatin1String("rolled-back")) out.result = InstallOutcome::Result::RolledBack;
            else if (v == QLatin1String("cancelled")) out.result = InstallOutcome::Result::Cancelled;
            else return fail(QStringLiteral("unknown result '%1'").arg(v.left(40)));
            haveResult = true;
        } else if (key == QLatin1String("step")) {
            out.step = value.trimmed();
        } else if (key == QLatin1String("reason")) {
            out.reason = value.trimmed();
        } else if (key == QLatin1String("version")) {
            out.version = value.trimmed();
        } else if (key == QLatin1String("code")) {
            out.code = value.trimmed();
        } else if (key == QLatin1String("attempt")) {
            out.attempt = value.trimmed();
        }
    }
    if (!haveResult) {
        return fail(QStringLiteral("result file has no 'result' line"));
    }
    if (errorOut) errorOut->clear();
    return out;
}

std::optional<InstallOutcome> readInstallOutcome(const QString& path,
                                                 QString* errorOut)
{
    // The family's three guards (readFileGuarded): a regular file, a sane
    // reported size, and never more than limit+1 bytes read — a device
    // reports size 0 and delivers forever.
    const QFileInfo fi(path);
    if (!fi.exists()) {
        if (errorOut) *errorOut = QStringLiteral("no result file");
        return std::nullopt;
    }
    if (!fi.isFile()) {
        if (errorOut) *errorOut = QStringLiteral("result path is not a regular file");
        return std::nullopt;
    }
    if (fi.size() > kInstallOutcomeMaxBytes) {
        if (errorOut) {
            *errorOut = QStringLiteral("result file larger than %1 bytes")
                            .arg(kInstallOutcomeMaxBytes);
        }
        return std::nullopt;
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (errorOut) *errorOut = QStringLiteral("cannot open result file: %1").arg(fileErrorName(f.error()));
        return std::nullopt;
    }
    const QByteArray bytes = f.read(kInstallOutcomeMaxBytes + 1);
    return parseInstallOutcome(bytes, errorOut);
}

QByteArray serializeInstallOutcome(const InstallOutcome& outcome)
{
    const auto oneLine = [](QString s) {
        s.replace(QLatin1Char('\r'), QLatin1Char(' '));
        s.replace(QLatin1Char('\n'), QLatin1Char(' '));
        return s;
    };
    QString text;
    text += QStringLiteral("result=") + installOutcomeResultName(outcome.result) + QLatin1Char('\n');
    if (!outcome.step.isEmpty())    text += QStringLiteral("step=") + oneLine(outcome.step) + QLatin1Char('\n');
    if (!outcome.reason.isEmpty())  text += QStringLiteral("reason=") + oneLine(outcome.reason) + QLatin1Char('\n');
    if (!outcome.version.isEmpty()) text += QStringLiteral("version=") + oneLine(outcome.version) + QLatin1Char('\n');
    if (!outcome.code.isEmpty())    text += QStringLiteral("code=") + oneLine(outcome.code) + QLatin1Char('\n');
    if (!outcome.attempt.isEmpty()) text += QStringLiteral("attempt=") + oneLine(outcome.attempt) + QLatin1Char('\n');
    return text.toUtf8();
}

// ---- The stage-1 backend: system installer via InstallerLauncher ---------------

namespace {

// Size first (cheap), then the SHA-256 streamed in blocks. The verified file
// may have been changed since the download — this is the check right before
// it leaves our hands (the update contract: "unmittelbar vor dem Ersetzen").
QString verifyFile(const QString& path, qint64 expectedSize,
                   const QString& expectedSha256)
{
    const QFileInfo fi(path);
    if (!fi.isFile()) {
        return QStringLiteral("verified file is missing or not a regular file");
    }
    if (expectedSize > 0 && fi.size() != expectedSize) {
        return QStringLiteral("verified file has %1 bytes, manifest says %2")
            .arg(fi.size()).arg(expectedSize);
    }
    if (expectedSha256.isEmpty()) {
        return QStringLiteral("order carries no expected SHA-256");
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return QStringLiteral("cannot open verified file: %1").arg(fileErrorName(f.error()));
    }
    QCryptographicHash h(QCryptographicHash::Sha256);
    qint64 seen = 0;
    while (!f.atEnd()) {
        const QByteArray block = f.read(1 << 20);
        if (block.isEmpty()) break;
        seen += block.size();
        // Never hash past what the manifest allowed — the file may grow
        // between the size check and here.
        if (expectedSize > 0 && seen > expectedSize) {
            return QStringLiteral("verified file grew past the manifest size");
        }
        h.addData(block);
    }
    const QString got = QString::fromLatin1(h.result().toHex());
    if (got.compare(expectedSha256, Qt::CaseInsensitive) != 0) {
        return QStringLiteral("SHA-256 of the verified file no longer matches the manifest");
    }
    return QString();
}

class SystemInstallerBackend final : public InstallBackend {
public:
    InstallCapability capability(const InstallContext& context) const override
    {
        InstallCapability cap;
        const QString k = context.kind.toLower();
        if (k == QLatin1String("dmg") || k == QLatin1String("msi")) {
            cap.method = InstallMethod::SystemInstaller;
            cap.reason = ManualReason::None;
            cap.detail = QStringLiteral("hands the file to the system installer");
            return cap;
        }
        if (k == QLatin1String("appimage")) {
            if (context.appLocation.isEmpty()) {
                cap.method = InstallMethod::Manual;
                cap.reason = ManualReason::NotAnAppImage;
                cap.detail = QStringLiteral("the app is not running from an AppImage");
                return cap;
            }
            const QFileInfo dir(QFileInfo(context.appLocation).absolutePath());
            if (!dir.isWritable()) {
                cap.method = InstallMethod::Manual;
                cap.reason = ManualReason::LocationNotWritable;
                cap.detail = QStringLiteral("the folder of the AppImage is not writable");
                return cap;
            }
            cap.method = InstallMethod::SystemInstaller;
            cap.reason = ManualReason::None;
            cap.detail = QStringLiteral("replaces the AppImage, runs on the next start");
            return cap;
        }
        cap.method = InstallMethod::Manual;
        cap.reason = ManualReason::UnsupportedKind;
        cap.detail = QStringLiteral("no installer for artifact kind '%1'").arg(context.kind.left(20));
        return cap;
    }

    void prepare(const InstallOrder& order, PrepareDone done) override
    {
        // The work is small here (one hash); the answer still comes on the
        // next turn of the event loop, so the flow sees the same asynchronous
        // shape it will see from SelfInstall.
        PrepareResult r = prepareNow(order);
        // The answer is bound to a shared ticket, not to `this`: the flow may
        // replace the backend before the event loop turns.
        const int ticket = ++*m_ticket;
        QTimer::singleShot(0, [token = m_ticket, ticket, r = std::move(r),
                               done = std::move(done)]() mutable {
            if (*token != ticket) return;  // cancelPrepare() or destroyed first
            done(std::move(r));
        });
    }

    void cancelPrepare() override { ++*m_ticket; }

    ~SystemInstallerBackend() override { *m_ticket = -1; }

    PrepareResult prepareNow(const InstallOrder& order)
    {
        PrepareResult r;
        const InstallCapability cap = capability(order.context);
        if (cap.method == InstallMethod::Manual) {
            r.manual = cap.reason;
            r.error = cap.detail;
            return r;
        }
        const QString bad = verifyFile(order.verifiedFile, order.expectedSize,
                                       order.expectedSha256);
        if (!bad.isEmpty()) {
            r.error = bad;
            return r;
        }
        const auto plan = installerLaunchPlan(order.verifiedFile,
                                              order.context.kind,
                                              order.context.appLocation);
        if (!plan) {
            r.manual = ManualReason::UnsupportedKind;
            r.error = QStringLiteral("no launch plan for kind '%1'").arg(order.context.kind.left(20));
            return r;
        }
        PreparedInstall p;
        p.order = order;
        p.method = cap.method;
        p.stagedFile = order.verifiedFile;  // nothing staged
        p.description = plan->description;
        r.prepared = p;
        return r;
    }

    LaunchResult launch(const PreparedInstall& prepared) override
    {
        LaunchResult r;
        // Checked again right before the hand-over: between prepare() and
        // aboutToQuit lies the app's own "unsaved work?" prompt, and the file
        // sits in a folder others may write to. What is left is the moment
        // between this check and the installer reading the file; closing that
        // needs a private staged copy (the self-installing backend does it).
        const QString bad = verifyFile(prepared.stagedFile, prepared.order.expectedSize,
                                       prepared.order.expectedSha256);
        if (!bad.isEmpty()) {
            r.error = bad;
            return r;
        }
        const auto plan = installerLaunchPlan(prepared.stagedFile,
                                              prepared.order.context.kind,
                                              prepared.order.context.appLocation);
        if (!plan) {
            r.error = QStringLiteral("no launch plan");
            return r;
        }
        QString err;
        if (plan->selfReplace) {
            r.started = replaceAppImage(prepared.stagedFile,
                                        prepared.order.context.appLocation, &err);
        } else {
            r.started = QProcess::startDetached(plan->program, plan->arguments);
            if (!r.started) err = QStringLiteral("failed to start %1").arg(plan->program);
        }
        r.error = err;
        return r;
    }

    void discard(const PreparedInstall&) override {}

private:
    std::shared_ptr<int> m_ticket = std::make_shared<int>(0);
};

}  // namespace

std::unique_ptr<InstallBackend> makeSystemInstallerBackend()
{
    return std::make_unique<SystemInstallerBackend>();
}

}  // namespace appupdate

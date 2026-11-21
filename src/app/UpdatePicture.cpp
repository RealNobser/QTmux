#include "UpdatePicture.h"

#include "update/InstallBackend.hpp"
#include "update/UpdateFlow.hpp"
#include "update/UpdateShowcase.hpp"

namespace qtmux {

namespace {

constexpr char kNarrow[] = "-narrow";
constexpr char kOtherInstance[] = "qtmux-other-instance";
constexpr char kSettingsPort[] = "qtmux-settings-port";

// „Von Hand“, weil aus demselben Ort eine zweite Instanz läuft. Startet nie etwas,
// wie der Installationsweg des Szenen-Bausteins.
class OtherInstanceBackend final : public appupdate::InstallBackend {
public:
    appupdate::InstallCapability capability(const appupdate::InstallContext &) const override
    {
        appupdate::InstallCapability c;
        c.method = appupdate::InstallMethod::Manual;
        c.reason = appupdate::ManualReason::OtherInstanceRunning;
        c.detail = QStringLiteral("another process runs from QTmux.app (picture)");
        return c;
    }
    void prepare(const appupdate::InstallOrder &, PrepareDone) override {}
    void cancelPrepare() override {}
    appupdate::LaunchResult launch(const appupdate::PreparedInstall &) override
    {
        return {false, QStringLiteral("picture never launches")};
    }
    void discard(const appupdate::PreparedInstall &) override {}
};

} // namespace

UpdatePicture::UpdatePicture(const QString &scene, const QString &language,
                             const QString &installed, const QString &buildSuffix)
    : m_scene(scene)
{
    m_hubScene = scene;
    if (scene == QLatin1String(kOtherInstance)) {
        m_hubScene = QStringLiteral("14");
        m_otherInstance = true;
    } else if (scene == QLatin1String(kSettingsPort)) {
        m_hubScene = QStringLiteral("settings-manual");
        m_portText = QStringLiteral("70000");
    } else if (scene.startsWith(QLatin1String("hint-")) && scene.endsWith(QLatin1String(kNarrow))) {
        m_hubScene = scene.chopped(int(sizeof(kNarrow)) - 1);
        m_width = 720;
    }
    m_surface = appupdate::UpdateShowcase::surfaceFor(m_hubScene);

    appupdate::UpdateShowcase::Options o;
    o.productSlug = QStringLiteral("qtmux");
    o.productName = QStringLiteral("QTmux");
    o.installed = installed;
    o.buildSuffix = buildSuffix;
    o.language = language;
    // „Was endet“ im Zustand „Bereit“: eine laufende Sitzung, wie im Selbsttest.
    o.impact.runningSessions = 1;
    m_showcase = std::make_unique<appupdate::UpdateShowcase>(o);
}

UpdatePicture::~UpdatePicture() = default;

QStringList UpdatePicture::scenes()
{
    QStringList all = appupdate::UpdateShowcase::scenes();
    for (const QString &s : appupdate::UpdateShowcase::scenes())
        if (s.startsWith(QLatin1String("hint-"))) all << s + QLatin1String(kNarrow);
    all << QLatin1String(kOtherInstance) << QLatin1String(kSettingsPort);
    return all;
}

bool UpdatePicture::begin(std::function<void(bool ok)> reached)
{
    if (!scenes().contains(m_scene)) {
        qCritical("QTmux update-picture: unbekannte Szene \"%s\"; vorhanden: %s",
                  qPrintable(m_scene), qPrintable(scenes().join(QStringLiteral(", "))));
        return false;
    }
    const bool entered = m_showcase->enter(m_hubScene, std::move(reached));
    // Der Ablauf entsteht synchron in enter(); sein Installationsweg wird erst nach
    // dem Download gefragt, also nach dem ersten Lauf der Ereignisschleife.
    if (entered && m_otherInstance && m_showcase->flow())
        m_showcase->flow()->setInstallBackend(std::make_unique<OtherInstanceBackend>());
    return entered && m_showcase->flow() != nullptr;
}

appupdate::UpdateFlow *UpdatePicture::flow() const { return m_showcase->flow(); }

} // namespace qtmux

#include "update/UpdateManifest.hpp"

#include <QJsonDocument>
#include <QJsonObject>

namespace appupdate {

namespace {

UpdateArtifact artifactFromJson(const QJsonObject& o)
{
    UpdateArtifact a;
    a.url          = o.value(QStringLiteral("url")).toString();
    a.size         = static_cast<qint64>(o.value(QStringLiteral("size")).toDouble(0));
    a.sha256       = o.value(QStringLiteral("sha256")).toString().toLower();
    a.kind         = o.value(QStringLiteral("kind")).toString();
    a.minOsVersion = o.value(QStringLiteral("minOsVersion")).toString();
    a.minHw        = o.value(QStringLiteral("minHw")).toString();
    a.sig          = o.value(QStringLiteral("sig")).toString();
    return a;
}

}  // namespace

std::optional<UpdateManifest> UpdateManifest::fromJson(const QByteArray& bytes,
                                                       QString* errorOut)
{
    const auto fail = [errorOut](const QString& why) -> std::optional<UpdateManifest> {
        if (errorOut) *errorOut = why;
        return std::nullopt;
    };

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
    if (doc.isNull() || !doc.isObject()) {
        return fail(QStringLiteral("manifest is not a JSON object: %1")
                        .arg(parseError.errorString()));
    }
    const QJsonObject root = doc.object();

    UpdateManifest m;
    m.schema = root.value(QStringLiteral("schema")).toInt(0);
    if (m.schema != 1) {
        return fail(QStringLiteral("unsupported manifest schema %1 (expected 1)")
                        .arg(m.schema));
    }

    m.product = root.value(QStringLiteral("product")).toString();
    if (m.product.isEmpty()) {
        return fail(QStringLiteral("manifest carries no product"));
    }

    m.versionString = root.value(QStringLiteral("version")).toString();
    m.version = QVersionNumber::fromString(m.versionString);
    if (m.version.isNull()) {
        return fail(QStringLiteral("manifest version '%1' is not parseable")
                        .arg(m.versionString));
    }

    m.channel   = root.value(QStringLiteral("channel")).toString();
    m.published = root.value(QStringLiteral("published")).toString();

    const QJsonObject notes = root.value(QStringLiteral("notes")).toObject();
    m.notesDe = notes.value(QStringLiteral("de")).toString();
    m.notesEn = notes.value(QStringLiteral("en")).toString();

    const QJsonObject artifacts = root.value(QStringLiteral("artifacts")).toObject();
    for (auto it = artifacts.begin(); it != artifacts.end(); ++it) {
        m.artifacts.insert(it.key(), artifactFromJson(it.value().toObject()));
    }

    return m;
}

QString UpdateManifest::currentArtifactKey()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("win-x86_64");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macos-universal");
#else
    return QStringLiteral("linux-x86_64");
#endif
}

std::optional<UpdateArtifact> UpdateManifest::currentArtifact() const
{
    const auto it = artifacts.constFind(currentArtifactKey());
    if (it == artifacts.constEnd() || !it->isComplete()) {
        return std::nullopt;
    }
    return *it;
}

QString UpdateManifest::notes(const QString& lang) const
{
    if (lang.startsWith(QStringLiteral("de"), Qt::CaseInsensitive)) {
        return notesDe.isEmpty() ? notesEn : notesDe;
    }
    return notesEn.isEmpty() ? notesDe : notesEn;
}

}  // namespace appupdate

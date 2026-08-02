#pragma once

// appupdate::UpdateManifest — parsed view of one product's manifest.json
// (schema 1, shared across RAFTNG / MacPCAN / QTmux / DeskStarter /
// EnzoEnigmaNG / AstroCAN modules):
//
//   { schema, product, channel, version, published, notes{de,en},
//     artifacts{ <key>: {url,size,sha256,kind[,minOsVersion][,minHw][,sig]} } }
//
// The signature contract lives one layer up: UpdateChecker verifies the
// Ed25519 detached signature over the EXACT manifest bytes BEFORE this
// parser ever sees them. Partial artifact maps are legal (RAFTNG starts
// with win-x86_64 only) — a missing key for the running platform is "no
// package for this OS", not a parse error.

#include <QMap>
#include <QString>
#include <QVersionNumber>

#include <optional>

namespace appupdate {

struct UpdateArtifact {
    QString url;
    qint64 size = 0;
    QString sha256;        // lowercase hex
    QString kind;          // "msi" | "dmg" | "appimage" | "bin" | "enzoimg"
    QString minOsVersion;  // optional
    QString minHw;         // optional (ESP32 products)
    QString sig;           // optional detached artifact signature (hex)

    // A usable desktop artifact needs at least a URL and a hash to check
    // the download against; entries missing those are carried but unusable.
    [[nodiscard]] bool isComplete() const
    {
        return !url.isEmpty() && !sha256.isEmpty() && size > 0;
    }
};

class UpdateManifest {
public:
    // Parses and validates. Returns nullopt (and fills errorOut when given)
    // for invalid JSON, wrong schema, or a missing product/version.
    static std::optional<UpdateManifest> fromJson(const QByteArray& bytes,
                                                  QString* errorOut = nullptr);

    // The artifact key this build looks up: "win-x86_64",
    // "macos-universal" or "linux-x86_64".
    static QString currentArtifactKey();

    // The artifact for this platform, if the manifest carries a complete one.
    [[nodiscard]] std::optional<UpdateArtifact> currentArtifact() const;

    // Release notes in the requested language ("de"/"en"), falling back to
    // the other one so the dialog never shows an empty page.
    [[nodiscard]] QString notes(const QString& lang) const;

    int schema = 0;
    QString product;
    QString channel;
    QVersionNumber version;
    QString versionString;  // verbatim, for display ("1.2.0-beta1")
    QString published;      // ISO date string, verbatim
    QString notesDe;
    QString notesEn;
    QMap<QString, UpdateArtifact> artifacts;
};

}  // namespace appupdate

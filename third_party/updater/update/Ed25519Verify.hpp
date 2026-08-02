#pragma once

// appupdate — Ed25519 signature verification over raw bytes.
//
// Backed by the vendored Monocypher (src/update/ed25519/), specifically the
// *optional* crypto_ed25519_check(): RFC-8032 Ed25519 with SHA-512, which is
// what `openssl pkeyutl -sign -rawin` produces and what the publish tooling
// uploads. Core Monocypher's similarly named crypto_eddsa_check() is EdDSA
// over BLAKE2b and will NOT verify OpenSSL signatures — do not "simplify"
// to it.

#include <QByteArray>

namespace appupdate {

// True iff `signature64` (exactly 64 bytes) is a valid Ed25519 signature of
// `message` under `publicKey32` (exactly 32 raw bytes, i.e. the output of
// `openssl pkey -pubout -outform DER | tail -c 32`). Wrong sizes return
// false instead of asserting — callers feed network bytes.
bool verifyEd25519(const QByteArray& signature64,
                   const QByteArray& publicKey32,
                   const QByteArray& message);

}  // namespace appupdate

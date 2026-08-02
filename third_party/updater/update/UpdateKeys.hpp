#pragma once

// appupdate — the production update-signing public key (Ed25519, 32 raw
// bytes), owner-generated 2026-08-02.
//
// Counterpart of the private key at ~/.keys/updates_ed25519_private.pem,
// which tools/updates/publish.py signs manifests with. The private key
// never enters any repository (and mind the documented trap: rsync/scp do
// not respect .gitignore — keep it out of trees that get tarballed to
// build hosts).
//
// THESE BYTES ARE SHARED VERBATIM with the firmware side: AstroCore's
// libs/AstroCAN/ota/ota_public_key.h, DeskStarter's vendored copy and
// EnzoEnigmaNG's boot_pubkey.hpp must carry the identical 32 bytes — one
// signing key for every product. Regenerate one, regenerate all.
//
// To re-derive from the private key:
//   openssl pkey -in ~/.keys/updates_ed25519_private.pem \
//       -pubout -outform DER | tail -c 32 | xxd -i
//
// Tests do NOT use this key — UpdateChecker takes an explicit key via its
// constructor for fixtures. The one exception is the interop test, which
// deliberately runs the real chain: publish.py signs with the private key,
// the checker verifies with THESE bytes.

namespace appupdate {

inline constexpr unsigned char kUpdatePublicKey[32] = {
    0xca, 0x6d, 0xde, 0x36, 0xde, 0x97, 0xa8, 0x9b,
    0xd8, 0xbe, 0x8d, 0xae, 0x92, 0x3c, 0x7e, 0x9a,
    0x25, 0x01, 0x83, 0xe7, 0xe9, 0xab, 0x0f, 0xa9,
    0x73, 0x1e, 0x40, 0x93, 0x14, 0xb2, 0xe7, 0xcb,
};

}  // namespace appupdate

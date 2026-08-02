# Vendored: Monocypher 4.0.2 (Ed25519 subset)

Source: https://monocypher.org/download/monocypher-4.0.2.tar.gz
(vendored 2026-08-02). Four files, flattened into this directory:

- `monocypher.{c,h}` — upstream `src/`
- `monocypher-ed25519.{c,h}` — upstream `src/optional/`

License: dual 2-clause BSD / CC0 (see upstream `LICENCE.md`).

**Use `crypto_ed25519_check()` / `crypto_ed25519_sign()`** from the
optional files — that is RFC-8032 Ed25519 (SHA-512), interoperable with
`openssl pkeyutl -rawin` and the publish tooling. The similarly named
`crypto_eddsa_*` functions in core Monocypher are EdDSA over BLAKE2b and
will NOT verify OpenSSL-produced signatures.

Upstream SHA-256 (of the vendored originals, before any local change —
there are none):

- monocypher.c `afe2b098c8569577a84488e0b98d276d1fba6506adea68bb9241a52111734c59`
- monocypher-ed25519.c `7c9b16056cbd27521919e8a6f56a228808b9e718afc42e3d33f28c08e5abdee2`

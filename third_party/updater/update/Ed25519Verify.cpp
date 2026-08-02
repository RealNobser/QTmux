#include "update/Ed25519Verify.hpp"

#include "update/ed25519/monocypher-ed25519.h"

namespace appupdate {

bool verifyEd25519(const QByteArray& signature64,
                   const QByteArray& publicKey32,
                   const QByteArray& message)
{
    if (signature64.size() != 64 || publicKey32.size() != 32) {
        return false;
    }
    return crypto_ed25519_check(
               reinterpret_cast<const uint8_t*>(signature64.constData()),
               reinterpret_cast<const uint8_t*>(publicKey32.constData()),
               reinterpret_cast<const uint8_t*>(message.constData()),
               static_cast<size_t>(message.size())) == 0;
}

}  // namespace appupdate

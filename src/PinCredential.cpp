#include "PinCredential.h"

#include <random>
#include <utility>

#include "Sha256.h"

namespace {

// Seeded from std::random_device per call. Adequate for demonstration; not a
// cryptographically secure generator, which is noted in the README.
std::string randomSaltHex() {
    static const char* kHex = "0123456789abcdef";

    std::random_device device;
    std::mt19937_64 engine{device()};
    std::uniform_int_distribution<int> nibble{0, 15};

    std::string salt;
    salt.reserve(32);
    for (int i = 0; i < 32; ++i) {
        salt += kHex[nibble(engine)];
    }
    return salt;
}

}  // namespace

PinCredential makePinCredential(const std::string& pin) {
    PinCredential credential;
    credential.salt = randomSaltHex();
    credential.hash = crypto::sha256Hex(credential.salt + pin);
    return credential;
}

PinCredential restorePinCredential(std::string salt, std::string hash) {
    PinCredential credential;
    credential.salt = std::move(salt);
    credential.hash = std::move(hash);
    return credential;
}

bool verifyPin(const PinCredential& credential, const std::string& pin) {
    return crypto::sha256Hex(credential.salt + pin) == credential.hash;
}

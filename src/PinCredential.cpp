#include "PinCredential.h"

#include <random>
#include <utility>

#include "Sha256.h"

namespace {

// Draws directly from std::random_device rather than seeding a PRNG from it.
//
// The tempting idiom is `std::mt19937_64 engine{device()}`, and it is wrong
// here. std::random_device::result_type is 32 bits, so a single call yields at
// most 32 bits of seed; mt19937_64's state is then expanded deterministically
// from that one value. The result is a 128-bit-wide salt drawn from a keyspace
// of only 2^32 - about 4.3 billion possible salts, giving a 50% collision
// chance across roughly 77,000 accounts by the birthday bound. Drawing from
// the device directly gives the full 128 bits.
//
// std::random_device satisfies UniformRandomBitGenerator, so it feeds a
// distribution with no engine in between. 32 calls per account creation is
// irrelevant next to the SHA-256 that follows.
//
// Caveat worth knowing: the standard permits a deterministic implementation of
// std::random_device. MinGW-w64 GCC before 9.2 shipped it as a fixed-seed
// mt19937, which would silently give every account the same salt. The
// IdenticalPinsGetDifferentSaltsAndDifferentHashes test is the canary for that
// and should not be deleted as redundant.
std::string randomSaltHex() {
    static const char* kHex = "0123456789abcdef";

    std::random_device device;
    std::uniform_int_distribution<int> nibble{0, 15};

    std::string salt;
    salt.reserve(32);
    for (int i = 0; i < 32; ++i) {
        salt += kHex[nibble(device)];
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

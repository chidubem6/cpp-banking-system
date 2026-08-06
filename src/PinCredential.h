#ifndef PIN_CREDENTIAL_H
#define PIN_CREDENTIAL_H

#include <string>

// A stored PIN: a random salt plus SHA-256(salt + pin). The PIN itself is
// never retained.
//
// LIMITATION, stated deliberately: SHA-256 is a fast digest, and speed is
// exactly what an attacker wants when guessing a four-digit PIN. Production
// systems must use a deliberately slow key-derivation function such as
// Argon2, scrypt or bcrypt. This implementation demonstrates salting and
// one-way storage; it is not fit for real deployment.
struct PinCredential {
    std::string salt;   // 32 lowercase hex chars (16 random bytes)
    std::string hash;   // 64 lowercase hex chars
};

// Generates a fresh random salt and hashes the PIN with it.
PinCredential makePinCredential(const std::string& pin);

// Rebuilds a credential from values already persisted. Performs no hashing.
PinCredential restorePinCredential(std::string salt, std::string hash);

bool verifyPin(const PinCredential& credential, const std::string& pin);

#endif

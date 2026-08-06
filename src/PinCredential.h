#ifndef PIN_CREDENTIAL_H
#define PIN_CREDENTIAL_H

#include <string>

// A stored PIN: a random salt plus SHA-256(salt + pin). The PIN itself is
// never retained.
//
// LIMITATIONS, stated deliberately.
//
// SHA-256 is a fast digest, and speed is what an attacker wants when guessing
// a PIN. Production systems use a deliberately slow key-derivation function -
// Argon2, scrypt, bcrypt, or PBKDF2-HMAC-SHA256, the last being the only one
// buildable on the digest already in this repo.
//
// But a slow KDF is not the main defence here, and it is worth being precise
// about why: a four-digit PIN has only 10,000 possibilities. Even at a
// generous 100ms per hash, an attacker exhausts the entire space in about 17
// minutes. A KDF buys a constant factor against a keyspace that needs orders
// of magnitude. The real mitigations for a short PIN are rate limiting,
// attempt lockout (see Account::kMaxFailedAttempts), and a secret pepper held
// outside the data file. The salt's job is narrower: it stops one precomputed
// table from breaking every account at once. It is public by design.
struct PinCredential {
    std::string salt;   // 32 lowercase hex chars = 128 bits drawn per nibble
    std::string hash;   // 64 lowercase hex chars
};

// Generates a fresh random salt and hashes the PIN with it.
PinCredential makePinCredential(const std::string& pin);

// Rebuilds a credential from values already persisted. Performs no hashing.
PinCredential restorePinCredential(std::string salt, std::string hash);

bool verifyPin(const PinCredential& credential, const std::string& pin);

#endif

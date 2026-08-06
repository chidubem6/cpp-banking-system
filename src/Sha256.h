#ifndef SHA256_H
#define SHA256_H

#include <string>

namespace crypto {

// SHA-256 as specified in FIPS 180-4. Returns 64 lowercase hex characters.
//
// This is a general-purpose digest, NOT a password-hashing function. See
// PinCredential.h for why that distinction matters here.
std::string sha256Hex(const std::string& input);

}  // namespace crypto

#endif

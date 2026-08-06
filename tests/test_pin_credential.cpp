#include <gtest/gtest.h>

#include <set>
#include <string>

#include "PinCredential.h"
#include "Sha256.h"

TEST(PinCredential, VerifiesTheCorrectPin) {
    const auto credential = makePinCredential("1234");
    EXPECT_TRUE(verifyPin(credential, "1234"));
}

TEST(PinCredential, RejectsTheWrongPin) {
    const auto credential = makePinCredential("1234");
    EXPECT_FALSE(verifyPin(credential, "1235"));
    EXPECT_FALSE(verifyPin(credential, ""));
    EXPECT_FALSE(verifyPin(credential, "12345"));
}

// The PIN here is deliberately NOT hex. An all-hex PIN like "1234" can occur
// in a 64-char hex digest by chance - about 61 windows at 16^-4, roughly a
// 1-in-1000 false failure per run - which would make this test a flake with no
// diagnostic value. The salt assertion is also dropped: the salt is generated
// without ever seeing the PIN, so it could only ever fail by coincidence.
TEST(PinCredential, NeverStoresThePin) {
    const auto credential = makePinCredential("hunter2");
    EXPECT_EQ(std::string::npos, credential.hash.find("hunter2"));
}

// PinCredential.h documents the salt as lowercase hex and Storage relies on
// that (a non-hex salt would exercise the comma-escaping path for no reason).
// Size alone would pass for 32 spaces.
TEST(PinCredential, SaltAndHashContainOnlyLowercaseHex) {
    const auto credential = makePinCredential("1234");
    EXPECT_EQ(std::string::npos, credential.salt.find_first_not_of("0123456789abcdef"));
    EXPECT_EQ(std::string::npos, credential.hash.find_first_not_of("0123456789abcdef"));
}

// Weak, but it is the only observable signal that salts vary at all. A
// deterministic std::random_device - which the standard permits, and which
// MinGW-w64 GCC before 9.2 actually shipped - would collapse this to 1.
TEST(PinCredential, ManyCredentialsGetDistinctSalts) {
    std::set<std::string> salts;
    for (int i = 0; i < 200; ++i) {
        salts.insert(makePinCredential("1234").salt);
    }
    EXPECT_EQ(200u, salts.size());
}

// The point of a salt: two accounts with the same PIN must not end up with
// the same hash, so one precomputed table cannot break both.
TEST(PinCredential, IdenticalPinsGetDifferentSaltsAndDifferentHashes) {
    const auto a = makePinCredential("1234");
    const auto b = makePinCredential("1234");
    EXPECT_NE(a.salt, b.salt);
    EXPECT_NE(a.hash, b.hash);
}

// Holding the salt fixed isolates the PIN as the only variable.
TEST(PinCredential, SameSaltWithDifferentPinsGivesDifferentHashes) {
    const auto reference = makePinCredential("1234");
    const auto sameSaltOtherPin =
        restorePinCredential(reference.salt, crypto::sha256Hex(reference.salt + "9999"));
    EXPECT_NE(reference.hash, sameSaltOtherPin.hash);
    EXPECT_TRUE(verifyPin(sameSaltOtherPin, "9999"));
    EXPECT_FALSE(verifyPin(sameSaltOtherPin, "1234"));
}

TEST(PinCredential, SaltAndHashHaveExpectedShape) {
    const auto credential = makePinCredential("1234");
    EXPECT_EQ(32u, credential.salt.size());
    EXPECT_EQ(64u, credential.hash.size());
}

TEST(PinCredential, RestoredCredentialVerifiesTheSamePin) {
    const auto original = makePinCredential("4321");
    const auto restored = restorePinCredential(original.salt, original.hash);
    EXPECT_TRUE(verifyPin(restored, "4321"));
    EXPECT_FALSE(verifyPin(restored, "1234"));
}

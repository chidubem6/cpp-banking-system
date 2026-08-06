#include <gtest/gtest.h>

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

TEST(PinCredential, NeverStoresThePin) {
    const auto credential = makePinCredential("1234");
    EXPECT_EQ(std::string::npos, credential.hash.find("1234"));
    EXPECT_EQ(std::string::npos, credential.salt.find("1234"));
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

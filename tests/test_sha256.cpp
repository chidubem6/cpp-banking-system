#include <gtest/gtest.h>

#include <string>

#include "Sha256.h"

// Vectors published in FIPS 180-2 Appendix B. Testing a hand-written digest
// against the official specification is what makes it trustworthy: if any of
// these mismatch, the implementation is wrong, not the expectation.
TEST(Sha256, EmptyString) {
    EXPECT_EQ("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
              crypto::sha256Hex(""));
}

TEST(Sha256, Abc) {
    EXPECT_EQ("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
              crypto::sha256Hex("abc"));
}

TEST(Sha256, TwoBlockMessage) {
    EXPECT_EQ("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
              crypto::sha256Hex(
                  "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"));
}

TEST(Sha256, OneMillionAs) {
    EXPECT_EQ("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
              crypto::sha256Hex(std::string(1000000, 'a')));
}

TEST(Sha256, OutputIsAlwaysSixtyFourLowercaseHexChars) {
    const std::string digest = crypto::sha256Hex("anything");
    EXPECT_EQ(64u, digest.size());
    for (char c : digest) {
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) << "char: " << c;
    }
}

TEST(Sha256, DifferentInputsGiveDifferentDigests) {
    EXPECT_NE(crypto::sha256Hex("1234"), crypto::sha256Hex("1235"));
}

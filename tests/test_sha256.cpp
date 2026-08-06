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

// Every vector above is ASCII, which leaves the sign-extension path entirely
// unexercised: `char` is signed on both toolchains here, so a byte >= 0x80
// would be negative if it were ever used without the cast to unsigned char.
// An implementation can pass all four NIST vectors and still be wrong for any
// non-ASCII input - a UTF-8 name, say. Expected digests computed independently
// with Python's hashlib.
TEST(Sha256, HandlesHighBitBytesAndEmbeddedNuls) {
    const std::string input("\x00\xff\x80\x7f\xc3\xa9", 6);
    EXPECT_EQ("1cb0855612229d4c55bee2b67fad509e595f95cb9203284e437efb17553beae6",
              crypto::sha256Hex(input));
}

// Padding boundaries the published vectors miss: 55 bytes is the largest
// message whose padding still fits in one block, 64 is exactly one full block
// and forces a second block containing nothing but padding.
TEST(Sha256, HandlesPaddingBoundaryLengths) {
    EXPECT_EQ("9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318",
              crypto::sha256Hex(std::string(55, 'a')));
    EXPECT_EQ("ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb",
              crypto::sha256Hex(std::string(64, 'a')));
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

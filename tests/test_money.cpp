#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

#include "Money.h"

TEST(Money, DefaultsToZero) {
    EXPECT_EQ(0, Money{}.pence());
}

TEST(Money, ParsesWholePounds) {
    auto m = Money::fromString("12");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(1200, m->pence());
}

TEST(Money, ParsesTwoDecimalPlaces) {
    auto m = Money::fromString("12.34");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(1234, m->pence());
}

TEST(Money, ParsesOneDecimalPlaceAsTens) {
    auto m = Money::fromString("12.3");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(1230, m->pence());
}

TEST(Money, ParsesSubPoundAmounts) {
    auto m = Money::fromString("0.05");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(5, m->pence());
}

TEST(Money, RejectsMalformedInput) {
    EXPECT_FALSE(Money::fromString("").has_value());
    EXPECT_FALSE(Money::fromString("abc").has_value());
    EXPECT_FALSE(Money::fromString("12.345").has_value());   // too many decimals
    EXPECT_FALSE(Money::fromString("12.").has_value());      // trailing point
    EXPECT_FALSE(Money::fromString(".5").has_value());       // no leading digit
    EXPECT_FALSE(Money::fromString("-5").has_value());       // negative input
    EXPECT_FALSE(Money::fromString("1 2").has_value());
    EXPECT_FALSE(Money::fromString("12.3.4").has_value());
    EXPECT_FALSE(Money::fromString("99999999999999999999").has_value());  // overflow
}

// This is the test that would fail if money were stored as a double.
TEST(Money, ArithmeticIsExact) {
    auto tenPence = Money::fromString("0.10");
    auto twentyPence = Money::fromString("0.20");
    auto thirtyPence = Money::fromString("0.30");
    ASSERT_TRUE(tenPence && twentyPence && thirtyPence);

    auto sum = tenPence->tryAdd(*twentyPence);
    ASSERT_TRUE(sum.has_value());
    EXPECT_EQ(*thirtyPence, *sum);
    EXPECT_EQ(30, sum->pence());
}

TEST(Money, AccumulationDoesNotDrift) {
    Money total;
    auto tenPence = Money::fromPence(10);
    for (int i = 0; i < 10000; ++i) {
        auto next = total.tryAdd(tenPence);
        ASSERT_TRUE(next.has_value());
        total = *next;
    }
    EXPECT_EQ(100000, total.pence());   // exactly £1000.00
}

TEST(Money, FormatsWithTwoDecimalPlaces) {
    EXPECT_EQ("£12.34", Money::fromPence(1234).toString());
    EXPECT_EQ("£0.05", Money::fromPence(5).toString());
    EXPECT_EQ("£0.00", Money::fromPence(0).toString());
    EXPECT_EQ("£1500.00", Money::fromPence(150000).toString());
    EXPECT_EQ("-£12.34", Money::fromPence(-1234).toString());
}

TEST(Money, DetectsAdditionOverflow) {
    auto big = Money::fromPence(std::numeric_limits<std::int64_t>::max());
    EXPECT_FALSE(big.tryAdd(Money::fromPence(1)).has_value());
}

TEST(Money, DetectsSubtractionOverflow) {
    auto small = Money::fromPence(std::numeric_limits<std::int64_t>::min());
    EXPECT_FALSE(small.trySubtract(Money::fromPence(1)).has_value());
}

TEST(Money, SubtractionCanGoNegative) {
    auto result = Money::fromPence(100).trySubtract(Money::fromPence(250));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(-150, result->pence());
}

TEST(Money, Compares) {
    EXPECT_TRUE(Money::fromPence(100) < Money::fromPence(200));
    EXPECT_TRUE(Money::fromPence(200) > Money::fromPence(100));
    EXPECT_TRUE(Money::fromPence(100) == Money::fromPence(100));
    EXPECT_TRUE(Money::fromPence(100) <= Money::fromPence(100));
    EXPECT_TRUE(Money::fromPence(100) != Money::fromPence(101));
}

TEST(Money, IsPositiveOnlyForAmountsAboveZero) {
    EXPECT_TRUE(Money::fromPence(1).isPositive());
    EXPECT_FALSE(Money::fromPence(0).isPositive());
    EXPECT_FALSE(Money::fromPence(-1).isPositive());
}

#include "core/food/DebugInfo.h"

#include <gtest/gtest.h>

#include <limits>

using namespace beetroot::core;

// Expected strings are what java.text.DecimalFormat produces for AppleSkin's patterns.

TEST(FormatDecimal, HashPatternDropsTrailingZerosAndLeadingZero) {
    EXPECT_EQ(formatDecimal(0.f, PATTERN_HASH_2), "0");
    EXPECT_EQ(formatDecimal(5.f, PATTERN_HASH_2), "5");
    EXPECT_EQ(formatDecimal(4.f, PATTERN_HASH_2), "4");
    EXPECT_EQ(formatDecimal(5.5f, PATTERN_HASH_2), "5.5");
    EXPECT_EQ(formatDecimal(9.6f, PATTERN_HASH_2), "9.6"); // float 9.60000038
    EXPECT_EQ(formatDecimal(12.8f, PATTERN_HASH_2), "12.8");
    EXPECT_EQ(formatDecimal(0.5f, PATTERN_HASH_2), ".5"); // "#" has no minimum integer digit
    EXPECT_EQ(formatDecimal(0.25f, PATTERN_HASH_2), ".25");
    EXPECT_EQ(formatDecimal(0.004f, PATTERN_HASH_2), "0"); // rounds to nothing: a lone zero
    EXPECT_EQ(formatDecimal(19.999f, PATTERN_HASH_2), "20");
    EXPECT_EQ(formatDecimal(1234567.f, PATTERN_HASH_2), "1234567");
}

TEST(FormatDecimal, ZeroPatternAlwaysHasTwoDecimals) {
    EXPECT_EQ(formatDecimal(0.f, PATTERN_ZERO_2), "0.00");
    EXPECT_EQ(formatDecimal(0.5f, PATTERN_ZERO_2), "0.50");
    EXPECT_EQ(formatDecimal(1.f, PATTERN_ZERO_2), "1.00");
    EXPECT_EQ(formatDecimal(3.999f, PATTERN_ZERO_2), "4.00");
    EXPECT_EQ(formatDecimal(0.1f, PATTERN_ZERO_2), "0.10");
    EXPECT_EQ(formatDecimal(2.3456f, PATTERN_ZERO_2), "2.35");
}

TEST(FormatDecimal, RoundsTheExactBinaryValueHalfEven) {
    // Exact ties (representable in binary) go to the even neighbour.
    EXPECT_EQ(formatDecimal(0.125f, PATTERN_ZERO_2), "0.12");
    EXPECT_EQ(formatDecimal(0.375f, PATTERN_ZERO_2), "0.38");
    EXPECT_EQ(formatDecimal(0.625f, PATTERN_ZERO_2), "0.62");
    EXPECT_EQ(formatDecimal(2.375f, PATTERN_ZERO_2), "2.38");
    EXPECT_EQ(formatDecimal(0.125f, PATTERN_HASH_2), ".12");
    // Decimal "ties" that are not ties in binary round by their real value.
    EXPECT_EQ(formatDecimal(1.005f, PATTERN_ZERO_2), "1.00"); // 1.00499999523
    EXPECT_EQ(formatDecimal(0.015f, PATTERN_ZERO_2), "0.01"); // 0.01499999966
    EXPECT_EQ(formatDecimal(2.675f, PATTERN_ZERO_2), "2.67"); // 2.67499995232
    EXPECT_EQ(formatDecimal(0.035f, PATTERN_ZERO_2), "0.04"); // 0.03500000015
}

TEST(FormatDecimal, NegativeZeroHasNoSign) {
    EXPECT_EQ(formatDecimal(-0.f, PATTERN_HASH_2), "0");
    EXPECT_EQ(formatDecimal(-0.f, PATTERN_ZERO_2), "0.00");
    EXPECT_EQ(formatDecimal(-0.004f, PATTERN_HASH_2), "0");
    EXPECT_EQ(formatDecimal(-0.004f, PATTERN_ZERO_2), "0.00");
    EXPECT_EQ(formatDecimal(-1.5f, PATTERN_HASH_2), "-1.5");
    EXPECT_EQ(formatDecimal(-0.5f, PATTERN_HASH_2), "-.5");
    EXPECT_EQ(formatDecimal(-0.006f, PATTERN_ZERO_2), "-0.01");
}

TEST(FormatDecimal, NonFiniteAndHuge) {
    EXPECT_EQ(formatDecimal(std::numeric_limits<float>::quiet_NaN(), PATTERN_ZERO_2), "NaN");
    EXPECT_EQ(formatDecimal(std::numeric_limits<float>::infinity(), PATTERN_HASH_2), "\xE2\x88\x9E");
    EXPECT_EQ(formatDecimal(-std::numeric_limits<float>::infinity(), PATTERN_HASH_2), "-\xE2\x88\x9E");
    EXPECT_EQ(formatDecimal(1e20f, PATTERN_ZERO_2), "100000002004087734272.00"); // exact float value
}

TEST(FormatDecimal, OtherPatterns) {
    EXPECT_EQ(formatDecimal(1.5f, {1, 0, 0}), "2");
    EXPECT_EQ(formatDecimal(2.5f, {1, 0, 0}), "2");
    EXPECT_EQ(formatDecimal(0.4f, {0, 0, 0}), "0");
    EXPECT_EQ(formatDecimal(0.1234567f, {1, 1, 9}), "0.123457"); // maxFractionDigits clamped to 6
}

TEST(FoodDebugLine, MatchesAppleSkinFormat) {
    EXPECT_EQ(foodDebugLine(20, 5.f, 0.f), "hunger: 20, sat: 5, exh: 0.00/4");
    EXPECT_EQ(foodDebugLine(17, 0.6f, 2.375f), "hunger: 17, sat: .6, exh: 2.38/4");
    EXPECT_EQ(foodDebugLine(0, 0.f, 3.999f), "hunger: 0, sat: 0, exh: 4.00/4");
    EXPECT_EQ(foodDebugLine(6, 12.8f, 1.f, 4.5f), "hunger: 6, sat: 12.8, exh: 1.00/4.5");

    FoodState s;
    s.hunger     = 14;
    s.saturation = 3.25f;
    s.exhaustion = 0.1f;
    EXPECT_EQ(foodDebugLine(s), "hunger: 14, sat: 3.25, exh: 0.10/4");
}

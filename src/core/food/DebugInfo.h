#pragma once

// AppleSkin's food debug line ("hunger: 20, sat: 5, exh: 0.00/4"), formatted exactly like
// AppleSkin's DebugInfoHandler (java.text.DecimalFormat patterns "#.##" and "0.00").

#include "core/food/FoodMath.h"

#include <string>

namespace beetroot::core {

/// A java.text.DecimalFormat-like pattern without grouping: "#.##" is {0, 0, 2}, "0.00" is
/// {1, 2, 2}.
struct DecimalPattern {
    int minIntegerDigits  = 1; ///< 0 or 1: with 0, "0.5" is written ".5" (like Java's "#.##").
    int minFractionDigits = 0;
    int maxFractionDigits = 2; ///< 0..6.
};

inline constexpr DecimalPattern PATTERN_HASH_2 = {0, 0, 2}; ///< "#.##"
inline constexpr DecimalPattern PATTERN_ZERO_2 = {1, 2, 2}; ///< "0.00"

/// Format `value` like DecimalFormat(pattern).format((double)value): the exact binary value is
/// rounded HALF_EVEN to maxFractionDigits, trailing zeros beyond minFractionDigits are dropped.
/// Unlike Java, a value that rounds to zero never gets a minus sign ("0", not "-0").
/// NaN -> "NaN", infinities -> "∞" / "-∞" (Java's symbols).
[[nodiscard]] std::string formatDecimal(float value, DecimalPattern pattern);

/// AppleSkin's debug text: "hunger: <int>, sat: <#.##>, exh: <0.00>/<#.##>".
[[nodiscard]] std::string
foodDebugLine(int hunger, float saturation, float exhaustion, float maxExhaustion = MAX_EXHAUSTION);
[[nodiscard]] std::string foodDebugLine(FoodState const& state);

} // namespace beetroot::core

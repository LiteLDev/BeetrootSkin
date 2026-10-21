#include "core/food/DebugInfo.h"

#include <algorithm>
#include <charconv>
#include <cmath>

namespace beetroot::core {

namespace {

/// Round an integral-or-not double to an integer, ties to even, independent of the FPU mode.
double roundHalfEven(double x) {
    double fl   = std::floor(x);
    double diff = x - fl; // exact: x and fl share the exponent range
    if (diff > 0.5) return fl + 1;
    if (diff < 0.5) return fl;
    return std::fmod(fl, 2.0) == 0 ? fl : fl + 1;
}

} // namespace

std::string formatDecimal(float value, DecimalPattern pattern) {
    if (std::isnan(value)) return "NaN";
    if (std::isinf(value)) return value < 0 ? "-\xE2\x88\x9E" : "\xE2\x88\x9E";

    int const maxFrac = std::clamp(pattern.maxFractionDigits, 0, 6);
    int const minFrac = std::clamp(pattern.minFractionDigits, 0, maxFrac);

    // A float has a 24-bit significand and 10^6 < 2^20, so this product is exact in a double:
    // rounding sees the exact binary value, like DecimalFormat on (double)value.
    constexpr double POW10[] = {1, 10, 100, 1000, 10000, 100000, 1000000};
    double           scaled  = std::fabs(static_cast<double>(value)) * POW10[maxFrac];
    double           units   = roundHalfEven(scaled);

    char        buf[400]{};
    auto        res = std::to_chars(buf, buf + sizeof(buf), units, std::chars_format::fixed, 0);
    std::string digits(buf, res.ptr);

    if (digits.size() <= static_cast<std::size_t>(maxFrac)) {
        digits.insert(0, static_cast<std::size_t>(maxFrac) + 1 - digits.size(), '0');
    }
    std::string intPart  = digits.substr(0, digits.size() - static_cast<std::size_t>(maxFrac));
    std::string fracPart = digits.substr(digits.size() - static_cast<std::size_t>(maxFrac));
    while (fracPart.size() > static_cast<std::size_t>(minFrac) && fracPart.back() == '0') fracPart.pop_back();
    if (intPart == "0" && pattern.minIntegerDigits <= 0) intPart.clear();

    std::string out;
    if (value < 0 && units != 0) out += '-';
    if (intPart.empty() && fracPart.empty()) return out + "0";
    out += intPart;
    if (!fracPart.empty()) {
        out += '.';
        out += fracPart;
    }
    return out;
}

std::string foodDebugLine(int hunger, float saturation, float exhaustion, float maxExhaustion) {
    std::string s  = "hunger: ";
    s             += std::to_string(hunger);
    s             += ", sat: ";
    s             += formatDecimal(saturation, PATTERN_HASH_2);
    s             += ", exh: ";
    s             += formatDecimal(exhaustion, PATTERN_ZERO_2);
    s             += '/';
    s             += formatDecimal(maxExhaustion, PATTERN_HASH_2);
    return s;
}

std::string foodDebugLine(FoodState const& state) {
    return foodDebugLine(state.hunger, state.saturation, state.exhaustion);
}

} // namespace beetroot::core

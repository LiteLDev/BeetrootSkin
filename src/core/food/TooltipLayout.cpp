#include "core/food/TooltipLayout.h"

#include "core/food/FoodMath.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace beetroot::core::tooltip {

Outline outlineFor(int modifiedHunger, int defaultHunger, int i) noexcept {
    if (modifiedHunger < 0) return Outline::Negative;
    if (modifiedHunger > defaultHunger && defaultHunger <= i) return Outline::Extra;
    if (modifiedHunger > i + 1 || defaultHunger == modifiedHunger) return Outline::Normal;
    if (modifiedHunger == i + 1) return Outline::Partial;
    return Outline::Missing;
}

Rgba outlineColor(Outline o) noexcept {
    switch (o) {
    case Outline::Negative:
        return {1.f, 1.f, 1.f, 1.f};
    case Outline::Extra:
        return {0.06f, 0.32f, 0.02f, 1.f};
    case Outline::Normal:
        return {0.f, 0.f, 0.f, 1.f};
    case Outline::Partial:
        return {0.53f, 0.21f, 0.08f, 1.f};
    case Outline::Missing:
        return {0.62f, 0.f, 0.f, 0.5f};
    }
    return {};
}

int estimateTextWidth(std::string const& s) noexcept { return static_cast<int>(s.size()) * GLYPH_WIDTH; }

std::string overflowText(int signedBars) { return "x" + std::to_string(signedBars); }

int FoodTooltipLayout::width() const {
    int hungerW = hungerBars * HUNGER_ICON_SIZE;
    if (!hungerBarsText.empty()) hungerW += estimateTextWidth(hungerBarsText);
    int saturationW = saturationBars * SATURATION_ICON_SIZE;
    if (!saturationBarsText.empty()) saturationW += estimateTextWidth(saturationBarsText);
    return std::max(hungerW, saturationW) + 2; // right padding
}

namespace {
/// Inputs are clamped to +-VALUE_LIMIT (far beyond any real food).
constexpr float VALUE_LIMIT = 1e6f;

/// (int) Math.ceil(|value| / 2f), robust against non-finite input.
int barsFor(float value) noexcept {
    if (!std::isfinite(value)) return 0;
    return static_cast<int>(std::ceil(std::fabs(value) / 2.f));
}
} // namespace

FoodTooltipLayout
layoutFoodTooltip(int defaultHunger, float defaultSaturation, int modifiedHunger, float modifiedSaturation) {
    FoodTooltipLayout l;
    // Keep absurd (data-driven) values in a range where the int arithmetic below is defined.
    auto clampSaturation = [](float v) { return std::isfinite(v) ? std::clamp(v, -VALUE_LIMIT, VALUE_LIMIT) : 0.f; };
    defaultHunger        = std::clamp(defaultHunger, -static_cast<int>(VALUE_LIMIT), static_cast<int>(VALUE_LIMIT));
    modifiedHunger       = std::clamp(modifiedHunger, -static_cast<int>(VALUE_LIMIT), static_cast<int>(VALUE_LIMIT));
    defaultSaturation    = clampSaturation(defaultSaturation);
    modifiedSaturation   = clampSaturation(modifiedSaturation);

    int const   biggestHunger     = std::max(defaultHunger, modifiedHunger);
    float const biggestSaturation = std::max(defaultSaturation, modifiedSaturation);

    l.hungerBars = (std::abs(biggestHunger) + 1) / 2;
    if (l.hungerBars > MAX_BARS) {
        l.hungerBarsText = overflowText((biggestHunger < 0 ? -1 : 1) * l.hungerBars);
        l.hungerBars     = 1;
    }
    l.saturationBars = barsFor(biggestSaturation);
    if (l.saturationBars > MAX_BARS || l.saturationBars == 0) {
        l.saturationBarsText = overflowText((biggestSaturation < 0 ? -1 : 1) * l.saturationBars);
        l.saturationBars     = 1;
    }
    if (!l.visible()) return l;

    // Hunger row, right to left.
    int x = (l.hungerBars - 1) * HUNGER_ICON_SIZE;
    for (int i = 0; i < l.hungerBars * 2; i += 2) {
        HungerIcon icon;
        icon.x            = x;
        icon.outline      = outlineFor(modifiedHunger, defaultHunger, i);
        icon.defaultHalf  = defaultHunger - 1 == i;
        icon.showModified = modifiedHunger > i;
        icon.modifiedHalf = modifiedHunger - 1 == i;
        l.hunger.push_back(icon);
        x -= HUNGER_ICON_SIZE;
    }
    if (!l.hungerBarsText.empty()) {
        x            += 2 * HUNGER_ICON_SIZE;
        l.hungerText  = {l.hungerBarsText, x + 2 * OVERFLOW_TEXT_SCALE, 2 * OVERFLOW_TEXT_SCALE};
    }

    // Saturation row, right to left.
    float const absSaturation = std::fabs(modifiedSaturation);
    x                         = (l.saturationBars - 1) * SATURATION_ICON_SIZE;
    for (int i = 0; i < l.saturationBars * 2; i += 2) {
        float const    effective = (absSaturation - static_cast<float>(i)) / 2.f;
        SaturationIcon icon;
        icon.x     = x;
        icon.u     = sprites::tooltipSaturationU(effective);
        icon.v     = sprites::tooltipSaturationV(modifiedSaturation < 0);
        icon.faded = absSaturation <= static_cast<float>(i);
        l.saturation.push_back(icon);
        x -= SATURATION_ICON_SIZE;
    }
    if (!l.saturationBarsText.empty()) {
        x += 2 * SATURATION_ICON_SIZE;
        l.saturationText =
            {l.saturationBarsText, x + 2 * OVERFLOW_TEXT_SCALE, SATURATION_ROW_Y + 1 * OVERFLOW_TEXT_SCALE};
    }
    return l;
}

} // namespace beetroot::core::tooltip

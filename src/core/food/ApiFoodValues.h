#pragma once

// Conversions between the internal core::FoodValues and the public beetroot::api::FoodValues
// (include/beetrootskin/api/FoodValues.h) that FoodValuesEvent listeners see and change.

#include "beetrootskin/api/FoodValues.h"
#include "core/food/FoodMath.h"

namespace beetroot::core {

/// The public view of `values` (hunger, saturation gained, canAlwaysEat).
[[nodiscard]] api::FoodValues toApiFoodValues(FoodValues const& values) noexcept;

/// Listener-supplied hunger and saturation are clamped to +-MAX_API_FOOD_VALUE, so the icon math
/// (current + food) cannot overflow.
inline constexpr int MAX_API_FOOD_VALUE = 10000;

/// `base` with hunger, saturation and canAlwaysEat taken from `values` (what a FoodValuesEvent
/// listener left). Effects and the rotten flag stay those of `base`. The saturation modifier is
/// derived from the new values (kept when hunger and saturation are unchanged); a non-finite
/// saturation becomes 0, and both are clamped to +-MAX_API_FOOD_VALUE.
[[nodiscard]] FoodValues withApiFoodValues(FoodValues const& base, api::FoodValues const& values);

} // namespace beetroot::core

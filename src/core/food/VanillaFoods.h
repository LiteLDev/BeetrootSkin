#pragma once

#include "core/food/FoodMath.h"

#include <string_view>
#include <vector>

namespace beetroot::core {

/// Eat effects of vanilla Bedrock foods, by full item id ("minecraft:golden_apple"). Used when
/// the item's food component does not expose its effects (data-driven FoodItemComponent).
/// Unknown ids give an empty list. Values follow Bedrock (not Java) where they differ.
[[nodiscard]] std::vector<FoodEffect> vanillaFoodEffects(std::string_view itemId);

/// True if `effects` contains a harmful effect (AppleSkin's "rotten" textures).
[[nodiscard]] bool anyHarmful(std::vector<FoodEffect> const& effects) noexcept;

} // namespace beetroot::core

#pragma once

#include "core/food/FoodMath.h"

#include <optional>

class ItemStackBase;
class Player;

namespace beetroot::game {

/// Food values of one item of `stack`, or nullopt if it is not edible (or empty).
/// Reads the item's IFoodItemComponent (nutrition, saturation modifier, canAlwaysEat); for
/// legacy food components also the eat effects (Regeneration of golden apples, Hunger of
/// rotten flesh, ...), which decide FoodValues::rotten. `player` is reserved for
/// player-dependent values and may be null.
[[nodiscard]] std::optional<core::FoodValues> getFoodValues(ItemStackBase const& stack, Player const* player = nullptr);

} // namespace beetroot::game

#pragma once

#include "core/food/FoodMath.h"

#include <optional>

class ItemStackBase;
class Player;

namespace beetroot::game {

/// The item's own food values (AppleSkin's "default" values) of one item of `stack`, or nullopt
/// if it is not edible (or empty). Reads the item's IFoodItemComponent (nutrition, saturation
/// modifier, canAlwaysEat); for legacy food components also the eat effects (Regeneration of
/// golden apples, Hunger of rotten flesh, ...), which decide FoodValues::rotten. Posts no event.
[[nodiscard]] std::optional<core::FoodValues> getDefaultFoodValues(ItemStackBase const& stack);

/// Default and modified food values (AppleSkin FoodHelper.QueriedFoodResult).
struct QueriedFoodValues {
    core::FoodValues defaultValues;  ///< After FoodValuesEvent listeners (they may change both).
    core::FoodValues modifiedValues; ///< What every BeetrootSkin display uses.
};
/// getDefaultFoodValues() plus the public api::FoodValuesEvent (include/beetrootskin/api), whose
/// listeners may change the values. `player` defaults to the local player (passed to the event,
/// may still be null). Client thread.
[[nodiscard]] std::optional<QueriedFoodValues> queryFoodValues(ItemStackBase const& stack, Player* player = nullptr);

/// The modified food values of one item of `stack` (queryFoodValues().modifiedValues), or
/// nullopt if it is not edible (or empty). `player` defaults to the local player. Client thread.
[[nodiscard]] std::optional<core::FoodValues> getFoodValues(ItemStackBase const& stack, Player const* player = nullptr);

} // namespace beetroot::game

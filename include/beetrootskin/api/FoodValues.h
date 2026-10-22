#pragma once

// BeetrootSkin public API: food values (AppleSkin's FoodProperties as seen by its API).
//
// Everything under beetrootskin/api is usable from other LeviLamina mods: include the headers
// from bin/BeetrootSkin-sdk/include, link bin/BeetrootSkin-sdk/lib/BeetrootSkin.lib and declare
// a dependency on BeetrootSkin in your manifest.json so it loads (and registers its events)
// first. See README.md, section "API".

#include "beetrootskin/api/Macros.h"

#include <optional>

class ItemStackBase;
class Player;

namespace beetroot::api {

/// Version of this API. Bumped on incompatible changes; additions keep it.
inline constexpr int API_VERSION = 1;

/// What eating one item restores, as BeetrootSkin displays it.
struct FoodValues {
    /// Hunger points restored (nutrition, in half shanks).
    int hunger = 0;
    /// Saturation points gained. Bedrock: nutrition x saturation_modifier x 2 for vanilla foods.
    float saturation = 0.f;
    /// Edible even when the hunger bar is full (golden apples, chorus fruit, ...).
    bool canAlwaysEat = false;

    /// Values from Bedrock's item definition terms (nutrition, saturation_modifier).
    [[nodiscard]] static constexpr FoodValues fromModifier(int nutrition, float modifier, bool canAlwaysEat = false) {
        return {nutrition, static_cast<float>(nutrition) * modifier * 2.f, canAlwaysEat};
    }
    /// Bedrock saturation_modifier matching `saturation` (0 when hunger <= 0).
    [[nodiscard]] constexpr float saturationModifier() const {
        return hunger > 0 ? saturation / (2.f * static_cast<float>(hunger)) : 0.f;
    }

    friend constexpr bool operator==(FoodValues const&, FoodValues const&) = default;
};

/// Default (from the item) and modified (after every FoodValuesEvent listener) values of a food.
struct QueriedFood {
    FoodValues defaultFoodValues;
    FoodValues modifiedFoodValues;
    /// The food applies a harmful effect (hunger, poison, ...): BeetrootSkin draws green shanks.
    bool rotten = false;
};

/// API_VERSION of the running BeetrootSkin.
[[nodiscard]] BEETROOTSKIN_API int apiVersion() noexcept;

/// Food values of one item of `stack` as BeetrootSkin uses them (posts FoodValuesEvent), or
/// nullopt if the item is not edible. `player` defaults to the local player. Client thread only.
[[nodiscard]] BEETROOTSKIN_API std::optional<QueriedFood>
                               queryFoodValues(ItemStackBase const& stack, Player* player = nullptr);

} // namespace beetroot::api

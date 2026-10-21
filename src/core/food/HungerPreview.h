#pragma once

// AppleSkin's "hunger restored" HUD preview (HUDOverlayHandler HungerRestored / drawHungerOverlay)
// as pure logic: when to show it and which shanks to draw.

#include "core/food/FoodMath.h"

#include <vector>

namespace beetroot::core {

/// AppleSkin HungerRestored visibility: the overlay option is on, the player can be hurt (not
/// creative/spectator), the food restores hunger and the player can eat it (not full unless
/// the food can always be eaten).
[[nodiscard]] bool
shouldShowHungerPreview(bool overlayEnabled, FoodState const& state, FoodValues const& food) noexcept;

/// Which sprite an icon of the preview uses.
enum class ShankSprite {
    Full,       ///< hunger_full
    Half,       ///< hunger_half
    EffectFull, ///< hunger_effect_full (rotten food)
    EffectHalf, ///< hunger_effect_half (rotten food)
};

/// One shank of the preview: AppleSkin draws the empty background (hunger_background, or
/// hunger_effect_background for rotten food) at alpha * 0.25, then `sprite` at alpha.
struct HungerPreviewIcon {
    int         index  = 0; ///< Hunger bar icon index (0 = rightmost shank, hunger 1-2).
    ShankSprite sprite = ShankSprite::Full;
    bool        rotten = false;

    friend bool operator==(HungerPreviewIcon const&, HungerPreviewIcon const&) = default;
};

/// Icons of hungerPreviewIcons(hunger, food.hunger); the last one is a half shank when the
/// hunger after eating is odd (AppleSkin: i * 2 + 1 == modifiedFood).
[[nodiscard]] std::vector<HungerPreviewIcon> hungerPreviewIconList(int currentHunger, FoodValues const& food);

} // namespace beetroot::core

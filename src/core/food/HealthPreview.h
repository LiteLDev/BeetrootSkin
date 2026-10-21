#pragma once

// AppleSkin's "health restored" HUD overlay, as pure logic: when to show it and which heart
// icons to draw. The drawing itself lives in src/features/health_preview.

#include "core/food/FoodMath.h"

#include <vector>

namespace beetroot::core {

/// Largest number of hearts the overlay handles (AppleSkin: more than 1000 hearts, or a
/// non-finite health, disables it).
inline constexpr int MAX_HEALTH_PREVIEW_HEARTS = 1000;

/// AppleSkin shouldShowEstimatedHealth + the HealthOverlay render conditions for the player
/// state alone (the config option and the held food are checked by the caller): false for
/// creative/spectator, when riding, on Peaceful, at hunger >= 18 (natural regeneration already
/// running), with Poison/Wither/Regeneration, when dead, and for non-finite or huge
/// (> 1000 hearts) health.
[[nodiscard]] bool healthPreviewAllowed(FoodState const& state) noexcept;

/// One heart icon of the overlay.
struct HealthPreviewIcon {
    int  index = 0;     ///< Heart index (0 = health 1-2, like ui::HudContext::heartIcons).
    bool half  = false; ///< Draw a half heart (the estimated health ends in this icon's first half).

    friend bool operator==(HealthPreviewIcon const&, HealthPreviewIcon const&) = default;
};

/// What the overlay draws for one held food.
struct HealthPreviewPlan {
    std::vector<HealthPreviewIcon> icons;         ///< Ascending heart indices; empty = draw nothing.
    float                          newHealth = 0; ///< Estimated health after eating (= health if no icons).
};

/// Hearts to draw for eating `food` in `state`: healthPreviewIcons(health,
/// estimatedHealthAfterEating) with AppleSkin's half-heart rule (i * 2 + 1 == ceil(new health)).
/// No icons when healthPreviewAllowed is false, the food cannot be eaten or nothing is gained.
[[nodiscard]] HealthPreviewPlan healthPreviewPlan(FoodState const& state, FoodValues const& food);

/// Resting (un-jittered) y of each icon of a bar laid out in rows of `perRow` icons: the smallest
/// y of the icon's row (best effort: a row whose icons are all shaken keeps its shake). The HUD
/// hooks use core::RestingRows (core/geom/IconRows.h) instead, which learns the line over frames.
[[nodiscard]] std::vector<float> restingIconY(std::vector<float> const& ys, int perRow = ICONS_PER_ROW);

} // namespace beetroot::core

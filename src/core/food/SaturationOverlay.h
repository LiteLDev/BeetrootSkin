#pragma once

// AppleSkin's HUD saturation overlay (HUDOverlayHandler.drawSaturationOverlay) as pure data:
// which hunger-bar icons get a saturation outline and which sprite each one uses.

#include "core/food/FoodMath.h"

#include <vector>

namespace beetroot::core {

/// One saturation outline icon over hunger icon `index` (0 = rightmost shank), drawn from
/// icons.png at (u, sprites::HUD_SATURATION_V), 9x9.
struct SaturationIcon {
    int index = 0;
    int u     = 0;

    friend bool operator==(SaturationIcon const&, SaturationIcon const&) = default;
};

/// Icons to draw for a bar showing `saturation + gained` (clamped to [0, 20]):
/// - gained == 0: the whole current saturation, icons 0 .. ceil(sat/2);
/// - gained != 0: only the gained part, starting at the icon holding the current saturation's
///   tail (saturation/2), so the preview redraws that partial icon with its new fill.
/// Each icon's u is sprites::hudSaturationU(modified/2 - index). Empty for negative or
/// non-finite totals.
[[nodiscard]] std::vector<SaturationIcon> saturationOverlaySprites(float saturation, float gained);

/// Saturation that eating `food` would add, as AppleSkin previews it: 0 if the food cannot be
/// eaten now, else eat(state, food).saturationGain (capped so the new saturation does not exceed
/// the new hunger level).
[[nodiscard]] float saturationPreviewGain(FoodState const& state, FoodValues const& food) noexcept;

} // namespace beetroot::core

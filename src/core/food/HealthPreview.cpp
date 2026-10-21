#include "core/food/HealthPreview.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace beetroot::core {

bool healthPreviewAllowed(FoodState const& s) noexcept {
    if (s.creativeLike || s.riding || s.peaceful) return false;
    if (s.hunger >= REGEN_MIN_HUNGER) return false;
    if (s.hasPoison || s.hasWither || s.hasRegeneration) return false;
    if (!std::isfinite(s.health) || !std::isfinite(s.maxHealth) || !(s.maxHealth > 0)) return false;
    if (!(s.health > 0)) return false; // dead (the HUD can still be drawn behind the death screen)
    // AppleSkin: ceil(maxHealth / 2) hearts outside [0, 1000] disables the overlay.
    if (std::ceil(s.maxHealth / 2.f) > static_cast<float>(MAX_HEALTH_PREVIEW_HEARTS)) return false;
    return true;
}

HealthPreviewPlan healthPreviewPlan(FoodState const& state, FoodValues const& food) {
    HealthPreviewPlan out;
    out.newHealth = state.health;
    if (!healthPreviewAllowed(state) || !canEat(state, food)) return out;
    float newHealth = estimatedHealthAfterEating(state, food);
    auto  range     = healthPreviewIcons(state.health, newHealth);
    if (range.empty()) return out;
    out.newHealth = newHealth;
    int fixedNew  = static_cast<int>(std::ceil(newHealth));
    out.icons.reserve(static_cast<std::size_t>(range.size()));
    for (int i = range.first; i < range.last; ++i) out.icons.push_back({i, isHalfIcon(i, fixedNew)});
    return out;
}

std::vector<float> restingIconY(std::vector<float> const& ys, int perRow) {
    std::vector<float> out(ys);
    if (perRow <= 0) return out;
    auto const step = static_cast<std::size_t>(perRow);
    for (std::size_t start = 0; start < ys.size(); start += step) {
        std::size_t end  = std::min(ys.size(), start + step);
        float       rest = ys[start];
        for (std::size_t i = start; i < end; ++i) rest = std::min(rest, ys[i]);
        for (std::size_t i = start; i < end; ++i) out[i] = rest;
    }
    return out;
}

} // namespace beetroot::core

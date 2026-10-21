#include "core/food/HungerPreview.h"

#include <algorithm>

namespace beetroot::core {

bool shouldShowHungerPreview(bool overlayEnabled, FoodState const& state, FoodValues const& food) noexcept {
    if (!overlayEnabled || state.creativeLike || food.hunger <= 0) return false;
    return canEat(state, food);
}

std::vector<HungerPreviewIcon> hungerPreviewIconList(int currentHunger, FoodValues const& food) {
    std::vector<HungerPreviewIcon> out;
    auto                           range = hungerPreviewIcons(currentHunger, food.hunger);
    if (range.empty()) return out;
    int modified = std::clamp(std::clamp(currentHunger, 0, MAX_HUNGER) + food.hunger, 0, MAX_HUNGER);
    out.reserve(static_cast<std::size_t>(range.size()));
    for (int i = range.first; i < range.last; ++i) {
        bool half = isHalfIcon(i, modified);
        auto s    = food.rotten ? (half ? ShankSprite::EffectHalf : ShankSprite::EffectFull)
                                : (half ? ShankSprite::Half : ShankSprite::Full);
        out.push_back({i, s, food.rotten});
    }
    return out;
}

} // namespace beetroot::core

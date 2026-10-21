#include "core/food/SaturationOverlay.h"

#include <algorithm>
#include <cmath>

namespace beetroot::core {

namespace {
/// Snap values within float noise of a whole number onto it, so a sum such as 0.1f + 9.9f that
/// should be exactly 10 doesn't round to 10.000001 and grow an extra (empty) icon in ceil().
float snapToWhole(float v) {
    float whole = std::round(v);
    return std::fabs(v - whole) < 1e-4f ? whole : v;
}
} // namespace

std::vector<SaturationIcon> saturationOverlaySprites(float saturation, float gained) {
    std::vector<SaturationIcon> out;
    if (!std::isfinite(saturation) || !std::isfinite(gained)) return out;
    float total = snapToWhole(saturation + gained);
    if (total < 0) return out;
    // AppleSkin drawSaturationOverlay: icons [start, ceil(modified / 2)).
    float modified = std::clamp(total, 0.f, MAX_SATURATION);
    int   first    = gained != 0 ? static_cast<int>(std::max(saturation / 2.f, 0.f)) : 0;
    int   last     = static_cast<int>(std::ceil(modified / 2.f));
    for (int i = first; i < last; ++i) {
        out.push_back({i, sprites::hudSaturationU(modified / 2.f - static_cast<float>(i))});
    }
    return out;
}

float saturationPreviewGain(FoodState const& state, FoodValues const& food) noexcept {
    if (!canEat(state, food)) return 0;
    float gain = eat(state, food).saturationGain;
    return std::isfinite(gain) && gain > 0 ? gain : 0;
}

} // namespace beetroot::core

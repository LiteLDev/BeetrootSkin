#include "core/food/ApiFoodValues.h"

#include <algorithm>
#include <cmath>

namespace beetroot::core {

api::FoodValues toApiFoodValues(FoodValues const& values) noexcept {
    return {values.hunger, values.saturation, values.canAlwaysEat};
}

FoodValues withApiFoodValues(FoodValues const& base, api::FoodValues const& values) {
    FoodValues out   = base;
    out.hunger       = std::clamp(values.hunger, -MAX_API_FOOD_VALUE, MAX_API_FOOD_VALUE);
    out.saturation   = std::isfinite(values.saturation) ? std::clamp(
                                                              values.saturation,
                                                              -static_cast<float>(MAX_API_FOOD_VALUE),
                                                              static_cast<float>(MAX_API_FOOD_VALUE)
                                                          )
                                                        : 0.f;
    out.canAlwaysEat = values.canAlwaysEat;
    if (out.hunger != base.hunger || out.saturation != base.saturation) {
        out.saturationModifier = out.hunger > 0 ? out.saturation / (2.f * static_cast<float>(out.hunger)) : 0.f;
    }
    return out;
}

} // namespace beetroot::core

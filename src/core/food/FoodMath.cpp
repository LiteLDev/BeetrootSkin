#include "core/food/FoodMath.h"

#include <algorithm>
#include <cmath>

namespace beetroot::core {

bool isHarmfulEffect(int id) noexcept {
    using namespace effect_id;
    switch (id) {
    case SLOWNESS:
    case MINING_FATIGUE:
    case INSTANT_DAMAGE:
    case NAUSEA:
    case BLINDNESS:
    case HUNGER:
    case WEAKNESS:
    case POISON:
    case WITHER:
    case LEVITATION:
    case FATAL_POISON:
    case DARKNESS:
        return true;
    default:
        return false;
    }
}

float FoodValues::saturationFor(int nutrition, float modifier) noexcept {
    return static_cast<float>(nutrition) * modifier * 2.f;
}

FoodValues FoodValues::make(int nutrition, float modifier, bool canAlwaysEat) {
    FoodValues v;
    v.hunger             = nutrition;
    v.saturationModifier = modifier;
    v.saturation         = saturationFor(nutrition, modifier);
    v.canAlwaysEat       = canAlwaysEat;
    return v;
}

bool canEat(FoodState const& state, FoodValues const& food) noexcept {
    return state.creativeLike || food.canAlwaysEat || state.hunger < MAX_HUNGER;
}

EatResult eat(FoodState const& state, FoodValues const& food) noexcept {
    EatResult r;
    r.newHunger      = std::clamp(state.hunger + food.hunger, 0, MAX_HUNGER);
    float sat        = std::max(state.saturation, 0.f);
    float target     = std::min(sat + std::max(food.saturation, 0.f), static_cast<float>(r.newHunger));
    r.newSaturation  = std::max(target, sat);
    r.hungerGained   = static_cast<float>(r.newHunger - state.hunger);
    r.saturationGain = r.newSaturation - sat;
    return r;
}

IconRange hungerPreviewIcons(int currentHunger, int foodHunger) noexcept {
    if (foodHunger <= 0) return {};
    int cur      = std::clamp(currentHunger, 0, MAX_HUNGER);
    int modified = std::clamp(cur + foodHunger, 0, MAX_HUNGER);
    return {cur / 2, (modified + 1) / 2};
}

IconRange saturationOverlayIcons(float saturation, float gained) noexcept {
    if (!std::isfinite(saturation) || !std::isfinite(gained) || saturation + gained < 0) return {};
    float modified = std::clamp(saturation + gained, 0.f, MAX_SATURATION);
    int   first    = gained != 0 ? static_cast<int>(std::max(saturation / 2.f, 0.f)) : 0;
    int   last     = static_cast<int>(std::ceil(modified / 2.f));
    return {first, std::max(first, last)};
}

IconRange healthPreviewIcons(float health, float newHealth) noexcept {
    if (!std::isfinite(health) || !std::isfinite(newHealth) || newHealth <= health) return {};
    int first = static_cast<int>(std::max(0.f, std::ceil(health) / 2.f));
    int last  = static_cast<int>(std::max(0.f, std::ceil(newHealth / 2.f)));
    return {first, std::max(first, last)};
}

bool isHalfIcon(int iconIndex, int value) noexcept { return iconIndex * 2 + 1 == value; }

float naturalRegenEstimate(int hunger, float saturation, float exhaustion) noexcept {
    if (!std::isfinite(saturation) || !std::isfinite(exhaustion)) return 0;
    hunger       = std::min(hunger, MAX_HUNGER);
    saturation   = std::clamp(saturation, 0.f, MAX_SATURATION);
    exhaustion   = std::clamp(exhaustion, 0.f, 1000.f);
    float health = 0;
    // Each iteration heals once (adding REGEN_EXHAUSTION_COST), so hunger strictly decreases
    // every few iterations: at most ~ (20 + 20 saturation) * 4 / 6 iterations.
    while (hunger >= REGEN_MIN_HUNGER) {
        while (exhaustion > MAX_EXHAUSTION) {
            exhaustion -= MAX_EXHAUSTION;
            if (saturation > 0) saturation = std::max(saturation - 1.f, 0.f);
            else --hunger;
        }
        if (hunger < REGEN_MIN_HUNGER) break;
        health     += 1;
        exhaustion += REGEN_EXHAUSTION_COST;
    }
    return health;
}

float regenerationEffectHealth(int amplifier, int durationTicks) noexcept {
    if (durationTicks <= 0) return 0;
    // Like `50 >> amplifier` on the JVM and on x86 (both shift by the low 5 bits), so levels
    // above 32 wrap around (minecraft.wiki "Regeneration": amplifiers are used modulo 32).
    // Negative amplifiers (never produced by the game) count as level I.
    int shift    = std::max(amplifier, 0) & 31;
    int interval = std::max(50 >> shift, 1);
    return std::floor(static_cast<float>(durationTicks) / static_cast<float>(interval));
}

float estimatedHealthIncrement(FoodState const& state, FoodValues const& food) noexcept {
    if (!(state.health < state.maxHealth)) return 0;
    float health = 0;
    auto  after  = eat(state, food);
    if (state.naturalRegen && !state.peaceful && after.newHunger >= REGEN_MIN_HUNGER) {
        health = naturalRegenEstimate(after.newHunger, after.newSaturation, state.exhaustion);
    }
    for (auto const& e : food.effects) {
        if (e.id != effect_id::REGENERATION) continue;
        health += regenerationEffectHealth(e.amplifier, e.durationTicks) * std::clamp(e.chance, 0.f, 1.f);
        break;
    }
    return health;
}

float estimatedHealthAfterEating(FoodState const& state, FoodValues const& food) noexcept {
    return std::min(state.maxHealth, state.health + estimatedHealthIncrement(state, food));
}

void FlashAnimator::tick(bool active) noexcept {
    if (!active) {
        reset();
        return;
    }
    mUnclamped += static_cast<float>(mDir) * STEP;
    if (mUnclamped >= HIGH) mDir = -1;
    else if (mUnclamped <= LOW) mDir = 1;
}

void FlashAnimator::reset() noexcept {
    mUnclamped = 0.f;
    mDir       = 1;
}

float FlashAnimator::alpha(float maxAlpha) const noexcept {
    if (!std::isfinite(maxAlpha)) maxAlpha = 0;
    return std::clamp(mUnclamped, 0.f, 1.f) * std::clamp(maxAlpha, 0.f, 1.f);
}

namespace sprites {

int hudSaturationU(float effective) noexcept {
    if (effective >= 1) return 3 * HUD_ICON_SIZE;
    if (effective > .5f) return 2 * HUD_ICON_SIZE;
    if (effective > .25f) return 1 * HUD_ICON_SIZE;
    return 0;
}

int tooltipSaturationU(float effective) noexcept {
    if (effective >= 1) return 3 * TOOLTIP_ICON_SIZE;
    if (effective > .5f) return 2 * TOOLTIP_ICON_SIZE;
    if (effective > .25f) return 1 * TOOLTIP_ICON_SIZE;
    if (effective > 0) return 0;
    return 4 * TOOLTIP_ICON_SIZE;
}

int tooltipSaturationV(bool negative) noexcept {
    return negative ? TOOLTIP_SATURATION_NEGATIVE_V : TOOLTIP_SATURATION_V;
}

int exhaustionBarWidth(float exhaustion, float maxExhaustion) noexcept {
    if (!std::isfinite(exhaustion) || !(maxExhaustion > 0)) return 0;
    float ratio = std::clamp(exhaustion / maxExhaustion, 0.f, 1.f);
    return static_cast<int>(ratio * static_cast<float>(EXHAUSTION_BAR_WIDTH));
}

} // namespace sprites

} // namespace beetroot::core

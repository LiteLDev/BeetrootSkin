#pragma once

// Pure food / hunger arithmetic shared by the HUD overlays, tooltips and debug info.
// Bedrock rules (not Java's) unless noted. No game types here; src/game converts game objects
// into these structs (game/FoodValues.h, game/PlayerFoodState.h).

#include <string>
#include <vector>

namespace beetroot::core {

/// Vanilla limits.
inline constexpr int   MAX_HUNGER            = 20;
inline constexpr float MAX_SATURATION        = 20.f;
inline constexpr float MAX_EXHAUSTION        = 4.f; ///< Exhaustion above this consumes saturation/hunger.
inline constexpr int   REGEN_MIN_HUNGER      = 18;  ///< Natural regeneration needs at least this much hunger.
inline constexpr int   REGEN_INTERVAL_TICKS  = 80;  ///< Bedrock natural regeneration: 1 HP per 80 ticks.
inline constexpr float REGEN_EXHAUSTION_COST = 6.f; ///< Exhaustion added per natural-regen heal.
inline constexpr int   ICONS_PER_ROW         = 10;  ///< Hunger/heart icons per row.

/// Well-known Bedrock mob effect ids (MobEffect::mId), for FoodEffect::id.
namespace effect_id {
inline constexpr int SPEED           = 1;
inline constexpr int SLOWNESS        = 2;
inline constexpr int HASTE           = 3;
inline constexpr int MINING_FATIGUE  = 4;
inline constexpr int STRENGTH        = 5;
inline constexpr int INSTANT_HEALTH  = 6;
inline constexpr int INSTANT_DAMAGE  = 7;
inline constexpr int JUMP_BOOST      = 8;
inline constexpr int NAUSEA          = 9;
inline constexpr int REGENERATION    = 10;
inline constexpr int RESISTANCE      = 11;
inline constexpr int FIRE_RESISTANCE = 12;
inline constexpr int WATER_BREATHING = 13;
inline constexpr int INVISIBILITY    = 14;
inline constexpr int BLINDNESS       = 15;
inline constexpr int NIGHT_VISION    = 16;
inline constexpr int HUNGER          = 17;
inline constexpr int WEAKNESS        = 18;
inline constexpr int POISON          = 19;
inline constexpr int WITHER          = 20;
inline constexpr int HEALTH_BOOST    = 21;
inline constexpr int ABSORPTION      = 22;
inline constexpr int SATURATION      = 23;
inline constexpr int LEVITATION      = 24;
inline constexpr int FATAL_POISON    = 25;
inline constexpr int SLOW_FALLING    = 27;
inline constexpr int BAD_OMEN        = 28;
inline constexpr int DARKNESS        = 30;
} // namespace effect_id

/// True for effects Java's MobEffectCategory::HARMFUL would contain (hunger, poison, nausea,
/// wither, slowness, ...). Used for FoodValues::rotten.
[[nodiscard]] bool isHarmfulEffect(int effectId) noexcept;

/// One effect a food applies when eaten.
struct FoodEffect {
    int         id            = 0; ///< Bedrock mob effect id (see effect_id).
    int         amplifier     = 0; ///< 0 = level I.
    int         durationTicks = 0;
    float       chance        = 1.f; ///< Probability in [0,1].
    std::string name;                ///< Effect name as the game knows it (may be empty).

    friend bool operator==(FoodEffect const&, FoodEffect const&) = default;
};

/// What eating one item does. Built by game::getFoodValues.
struct FoodValues {
    int                     hunger             = 0;     ///< Nutrition (half-shanks).
    float                   saturationModifier = 0.f;   ///< As defined by the item.
    float                   saturation         = 0.f;   ///< Saturation gained = hunger * modifier * 2 (Bedrock).
    bool                    canAlwaysEat       = false; ///< Edible even when the hunger bar is full.
    bool                    rotten             = false; ///< Applies at least one harmful effect.
    std::vector<FoodEffect> effects;

    /// Bedrock: saturation gained by eating = nutrition * saturationModifier * 2.
    [[nodiscard]] static float saturationFor(int nutrition, float modifier) noexcept;
    /// Convenience constructor that fills `saturation` via saturationFor.
    [[nodiscard]] static FoodValues make(int nutrition, float modifier, bool canAlwaysEat = false);

    friend bool operator==(FoodValues const&, FoodValues const&) = default;
};

/// The local player's food-relevant state. Built by game::readFoodState.
struct FoodState {
    int   hunger          = 20;
    float saturation      = 5.f;
    float exhaustion      = 0.f;
    float health          = 20.f;
    float maxHealth       = 20.f;
    bool  peaceful        = false;
    bool  hardcore        = false;
    bool  naturalRegen    = true;  ///< Game rule naturalregeneration.
    bool  creativeLike    = false; ///< Creative or spectator: no hunger.
    bool  riding          = false; ///< Riding a mount (vanilla hides the hunger bar for living mounts).
    bool  hasPoison       = false;
    bool  hasWither       = false;
    bool  hasRegeneration = false;
    bool  exact           = false; ///< Values come from the integrated server (exact) rather than the client copy.

    friend bool operator==(FoodState const&, FoodState const&) = default;
};

/// Java/AppleSkin canEat: creative-like players and canAlwaysEat foods can always eat,
/// everybody else only while the hunger bar is not full.
[[nodiscard]] bool canEat(FoodState const& state, FoodValues const& food) noexcept;

/// Result of eating `food` in `state`.
struct EatResult {
    int   newHunger      = 0; ///< min(20, hunger + food.hunger)
    float newSaturation  = 0; ///< min(newHunger, saturation + food.saturation), never below current
    float hungerGained   = 0; ///< newHunger - hunger
    float saturationGain = 0; ///< newSaturation - saturation (>= 0)
};
[[nodiscard]] EatResult eat(FoodState const& state, FoodValues const& food) noexcept;

/// Half-open icon index range [first, last) of the hunger bar affected by eating (AppleSkin:
/// from hunger/2 to ceil(min(20, hunger + food)/2)). Empty when food.hunger <= 0.
struct IconRange {
    int                first = 0;
    int                last  = 0;
    [[nodiscard]] bool empty() const noexcept { return last <= first; }
    [[nodiscard]] int  size() const noexcept { return last > first ? last - first : 0; }
    friend bool        operator==(IconRange const&, IconRange const&) = default;
};
[[nodiscard]] IconRange hungerPreviewIcons(int currentHunger, int foodHunger) noexcept;
/// Saturation bar icons to draw for `saturation + gained` (AppleSkin drawSaturationOverlay):
/// from saturation/2 (only when gained != 0, else 0) to ceil(clamp(sat+gained, 0, 20)/2).
[[nodiscard]] IconRange saturationOverlayIcons(float saturation, float gained) noexcept;
/// Heart icons affected when health rises from `health` to `newHealth` (half-hearts).
[[nodiscard]] IconRange healthPreviewIcons(float health, float newHealth) noexcept;
/// True if icon `i` of a bar showing `value` half-units is a half icon (value == 2i+1).
[[nodiscard]] bool isHalfIcon(int iconIndex, int value) noexcept;

/// HP that natural regeneration would restore, starting from (hunger, saturation, exhaustion),
/// until hunger drops below REGEN_MIN_HUNGER (Bedrock: no saturation fast-regen; each heal adds
/// REGEN_EXHAUSTION_COST exhaustion; exhaustion > 4 consumes saturation, then hunger).
/// Non-finite inputs give 0. Not capped by missing health.
[[nodiscard]] float naturalRegenEstimate(int hunger, float saturation, float exhaustion) noexcept;
/// HP restored by a Regeneration effect of `amplifier` lasting `durationTicks`
/// (floor(duration / max(50 >> (amplifier & 31), 1)); negative amplifiers count as 0). Bedrock uses the same intervals
/// as Java (Regen I 50 ticks, II 25, ...): e.g. the enchanted golden apple's Regen II for 30 s heals 24 HP
/// (minecraft.wiki "Regeneration").
[[nodiscard]] float regenerationEffectHealth(int amplifier, int durationTicks) noexcept;

/// Estimated HP gained by eating `food` (AppleSkin getEstimatedHealthIncrement for Bedrock):
/// 0 if not hurt; natural regen from the post-eat values (if naturalRegen, not peaceful and the
/// new hunger is >= 18) plus the first Regeneration effect (weighted by its chance).
/// Not capped; see estimatedHealthAfterEating for the capped value.
[[nodiscard]] float estimatedHealthIncrement(FoodState const& state, FoodValues const& food) noexcept;
/// min(maxHealth, health + estimatedHealthIncrement).
[[nodiscard]] float estimatedHealthAfterEating(FoodState const& state, FoodValues const& food) noexcept;

/// AppleSkin's pulsing overlay alpha. Call tick() once per client tick (20 Hz).
class FlashAnimator {
public:
    static constexpr float STEP = 0.125f;
    static constexpr float HIGH = 1.5f;
    static constexpr float LOW  = -0.5f;

    /// Advance one tick. `active` false (no food held) resets the animation.
    void tick(bool active) noexcept;
    void reset() noexcept;
    /// clamp(unclamped, 0, 1) * clamp(maxAlpha, 0, 1).
    [[nodiscard]] float alpha(float maxAlpha) const noexcept;
    [[nodiscard]] float unclamped() const noexcept { return mUnclamped; }
    [[nodiscard]] int   direction() const noexcept { return mDir; }

private:
    float mUnclamped = 0.f;
    int   mDir       = 1;
};

/// Sprite helpers for AppleSkin's icons.png (256x256). All values are texture pixels.
namespace sprites {
inline constexpr float ICONS_TEXTURE_SIZE = 256;
inline constexpr int   HUD_ICON_SIZE      = 9;
inline constexpr int   TOOLTIP_ICON_SIZE  = 7;
/// HUD saturation overlay row (9x9 icons at u = 0/9/18/27).
inline constexpr int HUD_SATURATION_V = 0;
/// Exhaustion underlay strip: 81x9 at (0, 18).
inline constexpr int EXHAUSTION_BAR_V     = 18;
inline constexpr int EXHAUSTION_BAR_WIDTH = 81;
/// Tooltip saturation icons (7x7): positive row / negative row.
inline constexpr int TOOLTIP_SATURATION_V          = 27;
inline constexpr int TOOLTIP_SATURATION_NEGATIVE_V = 34;

/// u of the HUD saturation icon for `effective` = saturation/2 - iconIndex:
/// >=1 -> 27, >0.5 -> 18, >0.25 -> 9, else 0.
[[nodiscard]] int hudSaturationU(float effective) noexcept;
/// u of the tooltip saturation icon for `effective` = |saturation|/2 - iconIndex:
/// >=1 -> 21, >0.5 -> 14, >0.25 -> 7, >0 -> 0, else 28 (empty outline).
[[nodiscard]] int tooltipSaturationU(float effective) noexcept;
/// v of the tooltip saturation row: 27 for positive values, 34 for negative ones.
[[nodiscard]] int tooltipSaturationV(bool negative) noexcept;
/// Width in pixels of the exhaustion underlay: (int)(clamp(exhaustion / 4, 0, 1) * 81).
[[nodiscard]] int exhaustionBarWidth(float exhaustion, float maxExhaustion = MAX_EXHAUSTION) noexcept;
} // namespace sprites

} // namespace beetroot::core

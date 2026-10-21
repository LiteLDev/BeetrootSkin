#include "core/food/FoodMath.h"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

using namespace beetroot::core;

namespace {
FoodState hungry(int hunger, float saturation = 0, float exhaustion = 0, float health = 10) {
    FoodState s;
    s.hunger     = hunger;
    s.saturation = saturation;
    s.exhaustion = exhaustion;
    s.health     = health;
    s.maxHealth  = 20;
    return s;
}
} // namespace

TEST(FoodValues, BedrockSaturationIsNutritionTimesModifierTimesTwo) {
    EXPECT_FLOAT_EQ(FoodValues::saturationFor(4, 1.2f), 9.6f);  // golden apple
    EXPECT_FLOAT_EQ(FoodValues::saturationFor(8, 0.8f), 12.8f); // steak
    EXPECT_FLOAT_EQ(FoodValues::saturationFor(4, 0.1f), 0.8f);  // rotten flesh
    auto v = FoodValues::make(6, 0.6f, true);
    EXPECT_EQ(v.hunger, 6);
    EXPECT_FLOAT_EQ(v.saturation, 7.2f);
    EXPECT_TRUE(v.canAlwaysEat);
    EXPECT_FALSE(v.rotten);
}

TEST(FoodValues, HarmfulEffects) {
    EXPECT_TRUE(isHarmfulEffect(effect_id::HUNGER));
    EXPECT_TRUE(isHarmfulEffect(effect_id::POISON));
    EXPECT_TRUE(isHarmfulEffect(effect_id::NAUSEA));
    EXPECT_TRUE(isHarmfulEffect(effect_id::WITHER));
    EXPECT_TRUE(isHarmfulEffect(effect_id::FATAL_POISON));
    EXPECT_FALSE(isHarmfulEffect(effect_id::REGENERATION));
    EXPECT_FALSE(isHarmfulEffect(effect_id::ABSORPTION));
    EXPECT_FALSE(isHarmfulEffect(effect_id::SATURATION));
    EXPECT_FALSE(isHarmfulEffect(0));
}

TEST(CanEat, FullBarOnlyWithCanAlwaysEatOrCreative) {
    auto food = FoodValues::make(4, 0.3f);
    EXPECT_TRUE(canEat(hungry(19), food));
    EXPECT_FALSE(canEat(hungry(20), food));
    food.canAlwaysEat = true;
    EXPECT_TRUE(canEat(hungry(20), food));
    food.canAlwaysEat     = false;
    auto creative         = hungry(20);
    creative.creativeLike = true;
    EXPECT_TRUE(canEat(creative, food));
}

TEST(Eat, HungerCappedAndSaturationCappedByNewHunger) {
    auto r = eat(hungry(10, 2), FoodValues::make(8, 0.8f)); // steak: +8, +12.8
    EXPECT_EQ(r.newHunger, 18);
    EXPECT_FLOAT_EQ(r.newSaturation, 14.8f);
    EXPECT_FLOAT_EQ(r.saturationGain, 12.8f);
    EXPECT_FLOAT_EQ(r.hungerGained, 8);

    r = eat(hungry(16, 4), FoodValues::make(8, 0.8f));
    EXPECT_EQ(r.newHunger, 20);
    EXPECT_FLOAT_EQ(r.newSaturation, 16.8f);

    r = eat(hungry(2, 0), FoodValues::make(4, 1.2f)); // 9.6 saturation, but only 6 hunger
    EXPECT_EQ(r.newHunger, 6);
    EXPECT_FLOAT_EQ(r.newSaturation, 6);
    EXPECT_FLOAT_EQ(r.saturationGain, 6);
}

TEST(Eat, SaturationNeverDecreases) {
    // Saturation above the new hunger (possible via /effect saturation) is kept.
    auto r = eat(hungry(4, 10), FoodValues::make(1, 0.1f));
    EXPECT_EQ(r.newHunger, 5);
    EXPECT_FLOAT_EQ(r.newSaturation, 10);
    EXPECT_FLOAT_EQ(r.saturationGain, 0);
}

TEST(HungerPreview, IconRange) {
    // 7 hunger (3.5 shanks) + 6 -> 13: icons 3..6 (index 3 is the half one being filled).
    EXPECT_EQ(hungerPreviewIcons(7, 6), (IconRange{3, 7}));
    EXPECT_EQ(hungerPreviewIcons(0, 1), (IconRange{0, 1}));
    EXPECT_EQ(hungerPreviewIcons(18, 8), (IconRange{9, 10}));
    EXPECT_EQ(hungerPreviewIcons(20, 4), (IconRange{10, 10}));
    EXPECT_TRUE(hungerPreviewIcons(20, 4).empty());
    EXPECT_TRUE(hungerPreviewIcons(5, 0).empty());
    EXPECT_TRUE(hungerPreviewIcons(5, -2).empty());
    EXPECT_EQ(hungerPreviewIcons(-3, 2), (IconRange{0, 1}));
}

TEST(HungerPreview, HalfIcons) {
    EXPECT_TRUE(isHalfIcon(6, 13));
    EXPECT_FALSE(isHalfIcon(6, 14));
    EXPECT_TRUE(isHalfIcon(0, 1));
}

TEST(SaturationOverlay, IconRange) {
    EXPECT_EQ(saturationOverlayIcons(5, 0), (IconRange{0, 3}));
    EXPECT_EQ(saturationOverlayIcons(5, 4), (IconRange{2, 5}));
    EXPECT_EQ(saturationOverlayIcons(0, 0), (IconRange{0, 0}));
    EXPECT_EQ(saturationOverlayIcons(18, 10), (IconRange{9, 10}));
    EXPECT_TRUE(saturationOverlayIcons(1, -5).empty());
    EXPECT_TRUE(saturationOverlayIcons(std::numeric_limits<float>::quiet_NaN(), 1).empty());
}

TEST(HealthPreview, IconRange) {
    EXPECT_EQ(healthPreviewIcons(10, 14), (IconRange{5, 7}));
    EXPECT_EQ(healthPreviewIcons(9, 12), (IconRange{4, 6}));
    EXPECT_EQ(healthPreviewIcons(9.5f, 11), (IconRange{5, 6}));
    EXPECT_TRUE(healthPreviewIcons(10, 10).empty());
    EXPECT_TRUE(healthPreviewIcons(10, 8).empty());
}

TEST(NaturalRegen, NoRegenBelowEighteen) {
    EXPECT_FLOAT_EQ(naturalRegenEstimate(17, 20, 0), 0);
    EXPECT_FLOAT_EQ(naturalRegenEstimate(0, 0, 0), 0);
}

TEST(NaturalRegen, NoSaturationAndNoExhaustion) {
    // hunger 18: heal (exh 6) -> consume 4 (hunger 17) -> stop. One heal.
    EXPECT_FLOAT_EQ(naturalRegenEstimate(18, 0, 0), 1);
    // hunger 20, sat 0: heals while hunger >= 18.
    // heal1 exh6 -> 2, h19; heal2 exh8 -> 4 (not > 4) ... let the simulation decide, check bounds.
    float h = naturalRegenEstimate(20, 0, 0);
    EXPECT_GE(h, 3);
    EXPECT_LE(h, 5);
}

TEST(NaturalRegen, SaturationAddsHeals) {
    float base = naturalRegenEstimate(20, 0, 0);
    float more = naturalRegenEstimate(20, 10, 0);
    EXPECT_GT(more, base);
    // Each saturation point absorbs 4 exhaustion = 2/3 heal.
    EXPECT_NEAR(more - base, 10 * 4.f / 6.f, 1.0f);
}

TEST(NaturalRegen, ExactSmallCase) {
    // 18 hunger, 1 saturation, 0 exhaustion:
    // heal (1) exh 6 -> -4 sat 0, exh 2 -> heal (2) exh 8 -> -4 hunger 17, exh 4 -> stop.
    EXPECT_FLOAT_EQ(naturalRegenEstimate(18, 1, 0), 2);
}

TEST(NaturalRegen, NonFiniteIsZeroAndHugeValuesTerminate) {
    EXPECT_FLOAT_EQ(naturalRegenEstimate(20, std::numeric_limits<float>::infinity(), 0), 0);
    EXPECT_FLOAT_EQ(naturalRegenEstimate(20, 0, std::numeric_limits<float>::quiet_NaN()), 0);
    EXPECT_LT(naturalRegenEstimate(1000000, 1e30f, 0), 100);
}

TEST(RegenerationEffect, HealthPerDuration) {
    EXPECT_FLOAT_EQ(regenerationEffectHealth(1, 100), 4);   // golden apple: Regen II 5 s
    EXPECT_FLOAT_EQ(regenerationEffectHealth(0, 100), 2);   // Regen I: every 50 ticks
    EXPECT_FLOAT_EQ(regenerationEffectHealth(4, 600), 200); // 50 >> 4 = 3
    EXPECT_FLOAT_EQ(regenerationEffectHealth(10, 20), 20);  // interval floors at 1
    EXPECT_FLOAT_EQ(regenerationEffectHealth(0, 0), 0);
    EXPECT_FLOAT_EQ(regenerationEffectHealth(-1, 100), 2);
}

TEST(HealthEstimate, NotHurtMeansZero) {
    auto s   = hungry(10, 0, 0, 20);
    auto apl = FoodValues::make(4, 1.2f);
    apl.effects.push_back({effect_id::REGENERATION, 1, 100, 1.f, "regeneration"});
    EXPECT_FLOAT_EQ(estimatedHealthIncrement(s, apl), 0);
}

TEST(HealthEstimate, RegenerationEffectOnly) {
    auto s   = hungry(4, 0, 0, 10); // too hungry for natural regen even after eating
    auto apl = FoodValues::make(4, 1.2f);
    apl.effects.push_back({effect_id::ABSORPTION, 0, 2400, 1.f, "absorption"});
    apl.effects.push_back({effect_id::REGENERATION, 1, 100, 1.f, "regeneration"});
    EXPECT_FLOAT_EQ(estimatedHealthIncrement(s, apl), 4);
    EXPECT_FLOAT_EQ(estimatedHealthAfterEating(s, apl), 14);
    apl.effects.back().chance = 0.5f;
    EXPECT_FLOAT_EQ(estimatedHealthIncrement(s, apl), 2);
}

TEST(HealthEstimate, NaturalRegenAfterEating) {
    auto  s     = hungry(14, 0, 0, 10);
    auto  steak = FoodValues::make(8, 0.8f);
    float inc   = estimatedHealthIncrement(s, steak);
    EXPECT_FLOAT_EQ(inc, naturalRegenEstimate(20, 12.8f, 0));
    EXPECT_GT(inc, 0);
    s.naturalRegen = false;
    EXPECT_FLOAT_EQ(estimatedHealthIncrement(s, steak), 0);
    s.naturalRegen = true;
    s.peaceful     = true;
    EXPECT_FLOAT_EQ(estimatedHealthIncrement(s, steak), 0);
}

TEST(HealthEstimate, CappedAtMaxHealth) {
    auto s     = hungry(14, 0, 0, 19);
    auto steak = FoodValues::make(8, 0.8f);
    EXPECT_FLOAT_EQ(estimatedHealthAfterEating(s, steak), 20);
}

TEST(FlashAnimator, CyclesBetweenBounds) {
    FlashAnimator f;
    EXPECT_FLOAT_EQ(f.alpha(1), 0);
    for (int i = 0; i < 8; ++i) f.tick(true);
    EXPECT_FLOAT_EQ(f.unclamped(), 1.0f);
    EXPECT_FLOAT_EQ(f.alpha(1), 1);
    EXPECT_FLOAT_EQ(f.alpha(0.65f), 0.65f);
    for (int i = 0; i < 4; ++i) f.tick(true);
    EXPECT_FLOAT_EQ(f.unclamped(), 1.5f);
    EXPECT_EQ(f.direction(), -1);
    EXPECT_FLOAT_EQ(f.alpha(1), 1);
    for (int i = 0; i < 16; ++i) f.tick(true);
    EXPECT_FLOAT_EQ(f.unclamped(), -0.5f);
    EXPECT_EQ(f.direction(), 1);
    EXPECT_FLOAT_EQ(f.alpha(1), 0);
    f.tick(true);
    EXPECT_FLOAT_EQ(f.unclamped(), -0.375f);
}

TEST(FlashAnimator, HalfAlpha) {
    FlashAnimator f;
    for (int i = 0; i < 4; ++i) f.tick(true);
    EXPECT_FLOAT_EQ(f.alpha(0.65f), 0.5f * 0.65f);
    EXPECT_FLOAT_EQ(f.alpha(2.f), 0.5f); // maxAlpha clamped
    EXPECT_FLOAT_EQ(f.alpha(-1.f), 0.f);
}

TEST(FlashAnimator, ResetWhenInactive) {
    FlashAnimator f;
    for (int i = 0; i < 13; ++i) f.tick(true);
    EXPECT_EQ(f.direction(), -1);
    f.tick(false);
    EXPECT_FLOAT_EQ(f.unclamped(), 0);
    EXPECT_EQ(f.direction(), 1);
}

TEST(Sprites, HudSaturationU) {
    EXPECT_EQ(sprites::hudSaturationU(1.0f), 27);
    EXPECT_EQ(sprites::hudSaturationU(3.0f), 27);
    EXPECT_EQ(sprites::hudSaturationU(0.75f), 18);
    EXPECT_EQ(sprites::hudSaturationU(0.5f), 9);
    EXPECT_EQ(sprites::hudSaturationU(0.3f), 9);
    EXPECT_EQ(sprites::hudSaturationU(0.25f), 0);
    EXPECT_EQ(sprites::hudSaturationU(0.1f), 0);
}

TEST(Sprites, TooltipSaturationUV) {
    EXPECT_EQ(sprites::tooltipSaturationU(1.0f), 21);
    EXPECT_EQ(sprites::tooltipSaturationU(0.6f), 14);
    EXPECT_EQ(sprites::tooltipSaturationU(0.5f), 7);
    EXPECT_EQ(sprites::tooltipSaturationU(0.26f), 7);
    EXPECT_EQ(sprites::tooltipSaturationU(0.25f), 0);
    EXPECT_EQ(sprites::tooltipSaturationU(0.01f), 0);
    EXPECT_EQ(sprites::tooltipSaturationU(0.f), 28);
    EXPECT_EQ(sprites::tooltipSaturationU(-1.f), 28);
    EXPECT_EQ(sprites::tooltipSaturationV(false), 27);
    EXPECT_EQ(sprites::tooltipSaturationV(true), 34);
}

TEST(Sprites, ExhaustionBarWidth) {
    EXPECT_EQ(sprites::exhaustionBarWidth(0), 0);
    EXPECT_EQ(sprites::exhaustionBarWidth(2), 40);
    EXPECT_EQ(sprites::exhaustionBarWidth(4), 81);
    EXPECT_EQ(sprites::exhaustionBarWidth(10), 81);
    EXPECT_EQ(sprites::exhaustionBarWidth(-1), 0);
    EXPECT_EQ(sprites::exhaustionBarWidth(1), 20);
    EXPECT_EQ(sprites::exhaustionBarWidth(std::numeric_limits<float>::quiet_NaN()), 0);
    EXPECT_EQ(sprites::exhaustionBarWidth(2, 0), 0);
}

#include "core/food/VanillaFoods.h"

TEST(VanillaFoods, KnownEffects) {
    auto apple = vanillaFoodEffects("minecraft:golden_apple");
    ASSERT_EQ(apple.size(), 2u);
    EXPECT_EQ(apple[0].id, effect_id::REGENERATION);
    EXPECT_EQ(apple[0].amplifier, 1);
    EXPECT_EQ(apple[0].durationTicks, 100);
    EXPECT_FALSE(anyHarmful(apple));

    auto flesh = vanillaFoodEffects("minecraft:rotten_flesh");
    ASSERT_EQ(flesh.size(), 1u);
    EXPECT_EQ(flesh[0].id, effect_id::HUNGER);
    EXPECT_FLOAT_EQ(flesh[0].chance, 0.8f);
    EXPECT_TRUE(anyHarmful(flesh));

    EXPECT_TRUE(anyHarmful(vanillaFoodEffects("minecraft:pufferfish")));
    EXPECT_TRUE(anyHarmful(vanillaFoodEffects("minecraft:chicken")));
    EXPECT_TRUE(vanillaFoodEffects("minecraft:bread").empty());
    EXPECT_TRUE(vanillaFoodEffects("golden_apple").empty()); // full ids only
    EXPECT_FALSE(anyHarmful({}));
}

TEST(VanillaFoods, GoldenAppleHealthEstimate) {
    FoodState s;
    s.hunger     = 4;
    s.health     = 10;
    s.maxHealth  = 20;
    auto food    = FoodValues::make(4, 1.2f, true);
    food.effects = vanillaFoodEffects("minecraft:golden_apple");
    EXPECT_FLOAT_EQ(estimatedHealthIncrement(s, food), 4);
}

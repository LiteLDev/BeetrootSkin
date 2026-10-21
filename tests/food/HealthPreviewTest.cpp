#include "core/food/HealthPreview.h"

#include <gtest/gtest.h>

#include <limits>

using namespace beetroot::core;

namespace {
FoodState survival(int hunger, float health, float saturation = 0, float maxHealth = 20) {
    FoodState s;
    s.hunger     = hunger;
    s.saturation = saturation;
    s.exhaustion = 0;
    s.health     = health;
    s.maxHealth  = maxHealth;
    return s;
}

FoodValues goldenApple() {
    auto v    = FoodValues::make(4, 1.2f, true);
    v.effects = {
        {effect_id::REGENERATION, 1, 100,  1.f, "regeneration"},
        {effect_id::ABSORPTION,   0, 2400, 1.f, "absorption"  }
    };
    return v;
}
} // namespace

TEST(HealthPreview, AllowedOnlyInPlainSurvivalBelowEighteenHunger) {
    auto s = survival(10, 8);
    EXPECT_TRUE(healthPreviewAllowed(s));

    auto t     = s;
    t.peaceful = true;
    EXPECT_FALSE(healthPreviewAllowed(t));
    t        = s;
    t.hunger = 18; // natural regeneration already running
    EXPECT_FALSE(healthPreviewAllowed(t));
    t.hunger = 17;
    EXPECT_TRUE(healthPreviewAllowed(t));
    t           = s;
    t.hasPoison = true;
    EXPECT_FALSE(healthPreviewAllowed(t));
    t           = s;
    t.hasWither = true;
    EXPECT_FALSE(healthPreviewAllowed(t));
    t                 = s;
    t.hasRegeneration = true;
    EXPECT_FALSE(healthPreviewAllowed(t));
    t        = s;
    t.riding = true;
    EXPECT_FALSE(healthPreviewAllowed(t));
    t              = s;
    t.creativeLike = true;
    EXPECT_FALSE(healthPreviewAllowed(t));
    t        = s;
    t.health = 0; // dead
    EXPECT_FALSE(healthPreviewAllowed(t));
}

TEST(HealthPreview, HugeOrNonFiniteHealthDisables) {
    auto s      = survival(10, 8);
    s.maxHealth = 2000; // exactly 1000 hearts is still fine
    EXPECT_TRUE(healthPreviewAllowed(s));
    s.maxHealth = 2002;
    EXPECT_FALSE(healthPreviewAllowed(s));
    s.maxHealth = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(healthPreviewAllowed(s));
    s.maxHealth = 20;
    s.health    = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(healthPreviewAllowed(s));
}

TEST(HealthPreview, SteakLiftsHungerToEighteenAndRegenerates) {
    // hunger 10 + steak 8 = 18, saturation 12.8: natural regen heals 10 HP (Bedrock: 6 exhaustion
    // per heal, 1.5 saturation each; the 10th heal drops hunger to 17).
    auto s     = survival(10, 8);
    auto steak = FoodValues::make(8, 0.8f);
    EXPECT_FLOAT_EQ(estimatedHealthIncrement(s, steak), 10);
    EXPECT_FLOAT_EQ(healthPreviewPlan(s, steak).newHealth, 18);
    EXPECT_FLOAT_EQ(healthPreviewPlan(s, FoodValues::make(5, 0.6f)).newHealth, 8); // bread: unchanged
    auto plan = healthPreviewPlan(s, steak).icons;
    ASSERT_EQ(plan.size(), 5u); // hearts 4..8 (health 9..18)
    EXPECT_EQ(plan.front(), (HealthPreviewIcon{4, false}));
    EXPECT_EQ(plan.back(), (HealthPreviewIcon{8, false}));
}

TEST(HealthPreview, BreadThatStaysBelowEighteenShowsNothing) {
    auto s     = survival(10, 8);
    auto bread = FoodValues::make(5, 0.6f); // 15 < 18: no natural regen
    EXPECT_TRUE(healthPreviewPlan(s, bread).icons.empty());
}

TEST(HealthPreview, GoldenAppleRegenerationEffect) {
    auto s    = survival(10, 8); // 10 + 4 = 14: no natural regen, Regen II 5 s = 4 HP
    auto plan = healthPreviewPlan(s, goldenApple()).icons;
    ASSERT_EQ(plan.size(), 2u);
    EXPECT_EQ(plan[0], (HealthPreviewIcon{4, false}));
    EXPECT_EQ(plan[1], (HealthPreviewIcon{5, false}));
}

TEST(HealthPreview, HalfHeartAtTheEnd) {
    auto s    = survival(10, 7); // 7 + 4 = 11: hearts 3 (7..8, already half) .. 5 (11, half)
    auto plan = healthPreviewPlan(s, goldenApple()).icons;
    ASSERT_EQ(plan.size(), 3u);
    EXPECT_EQ(plan[0], (HealthPreviewIcon{3, false}));
    EXPECT_EQ(plan[1], (HealthPreviewIcon{4, false}));
    EXPECT_EQ(plan[2], (HealthPreviewIcon{5, true}));
}

TEST(HealthPreview, CappedAtMaxHealthAndMultipleRows) {
    auto s    = survival(10, 22, 0, 40); // Health Boost: two rows of hearts
    auto plan = healthPreviewPlan(s, FoodValues::make(8, 0.8f)).icons;
    ASSERT_EQ(plan.size(), 5u); // 22 + 10 = 32 -> hearts 11..15 (second row)
    EXPECT_EQ(plan.front().index, 11);
    EXPECT_EQ(plan.back().index, 15);

    auto full   = survival(10, 19);
    auto capped = healthPreviewPlan(full, FoodValues::make(8, 0.8f)).icons;
    ASSERT_EQ(capped.size(), 1u);
    EXPECT_EQ(capped[0], (HealthPreviewIcon{9, false}));
}

TEST(HealthPreview, NothingWhenHiddenOrNotHurtOrNotEdible) {
    auto steak = FoodValues::make(8, 0.8f);
    auto s     = survival(10, 20);
    EXPECT_TRUE(healthPreviewPlan(s, steak).icons.empty()); // not hurt
    s          = survival(10, 8);
    s.peaceful = true;
    EXPECT_TRUE(healthPreviewPlan(s, steak).icons.empty());
    s              = survival(10, 8);
    s.naturalRegen = false;
    EXPECT_TRUE(healthPreviewPlan(s, steak).icons.empty());
    EXPECT_FALSE(healthPreviewPlan(s, goldenApple()).icons.empty()); // the effect still heals
}

TEST(HealthPreview, RestingIconYPerRow) {
    std::vector<float> ys{10, 11, 10, 10, 11, 10, 10, 10, 10, 11, 0, 1, 0};
    auto               rest = restingIconY(ys);
    ASSERT_EQ(rest.size(), ys.size());
    for (int i = 0; i < 10; ++i) EXPECT_FLOAT_EQ(rest[i], 10);
    for (int i = 10; i < 13; ++i) EXPECT_FLOAT_EQ(rest[i], 0);
    EXPECT_TRUE(restingIconY({}).empty());
    EXPECT_EQ(restingIconY({3, 4}, 0), (std::vector<float>{3, 4}));
}

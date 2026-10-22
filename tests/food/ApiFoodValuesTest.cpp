#include "core/food/ApiFoodValues.h"

#include <gtest/gtest.h>

#include <limits>

using namespace beetroot;
using namespace beetroot::core;

TEST(ApiFoodValues, FromModifierMatchesBedrockSaturation) {
    constexpr auto bread = api::FoodValues::fromModifier(5, 0.6f);
    EXPECT_EQ(bread.hunger, 5);
    EXPECT_FLOAT_EQ(bread.saturation, 6.f);
    EXPECT_FALSE(bread.canAlwaysEat);
    EXPECT_FLOAT_EQ(bread.saturationModifier(), 0.6f);
    EXPECT_FLOAT_EQ((api::FoodValues{0, 3.f, false}.saturationModifier()), 0.f);
    EXPECT_EQ(bread, (api::FoodValues{5, 6.f, false}));
}

TEST(ApiFoodValues, ToApiCopiesThePublicFields) {
    auto apple = FoodValues::make(4, 1.2f, true);
    auto v     = toApiFoodValues(apple);
    EXPECT_EQ(v.hunger, 4);
    EXPECT_FLOAT_EQ(v.saturation, apple.saturation);
    EXPECT_TRUE(v.canAlwaysEat);
}

TEST(ApiFoodValues, UnchangedValuesRoundTripExactly) {
    auto flesh   = FoodValues::make(4, 0.1f);
    flesh.rotten = true;
    flesh.effects.push_back({effect_id::HUNGER, 0, 600, 0.8f, "hunger"});
    EXPECT_EQ(withApiFoodValues(flesh, toApiFoodValues(flesh)), flesh);
}

TEST(ApiFoodValues, ModifiedValuesKeepEffectsAndRecomputeModifier) {
    auto flesh   = FoodValues::make(4, 0.1f);
    flesh.rotten = true;
    flesh.effects.push_back({effect_id::HUNGER, 0, 600, 0.8f, "hunger"});

    auto v   = toApiFoodValues(flesh);
    v.hunger = 8; // doubled nutrition, same saturation
    auto out = withApiFoodValues(flesh, v);
    EXPECT_EQ(out.hunger, 8);
    EXPECT_FLOAT_EQ(out.saturation, flesh.saturation);
    EXPECT_FLOAT_EQ(out.saturationModifier, flesh.saturation / 16.f);
    EXPECT_TRUE(out.rotten);
    EXPECT_EQ(out.effects, flesh.effects);

    v.canAlwaysEat = true;
    v.saturation   = 5.f;
    out            = withApiFoodValues(flesh, v);
    EXPECT_TRUE(out.canAlwaysEat);
    EXPECT_FLOAT_EQ(out.saturation, 5.f);
}

TEST(ApiFoodValues, NonFiniteSaturationAndZeroHunger) {
    auto beef    = FoodValues::make(8, 0.8f);
    auto v       = toApiFoodValues(beef);
    v.hunger     = 0;
    v.saturation = std::numeric_limits<float>::quiet_NaN();
    auto out     = withApiFoodValues(beef, v);
    EXPECT_EQ(out.hunger, 0);
    EXPECT_FLOAT_EQ(out.saturation, 0.f);
    EXPECT_FLOAT_EQ(out.saturationModifier, 0.f);
}

TEST(ApiFoodValues, ListenerValuesAreClamped) {
    auto beef    = FoodValues::make(8, 0.8f);
    auto v       = toApiFoodValues(beef);
    v.hunger     = std::numeric_limits<int>::max();
    v.saturation = -std::numeric_limits<float>::infinity();
    auto out     = withApiFoodValues(beef, v);
    EXPECT_EQ(out.hunger, MAX_API_FOOD_VALUE);
    EXPECT_FLOAT_EQ(out.saturation, 0.f);
    v.hunger     = std::numeric_limits<int>::min();
    v.saturation = 1e30f;
    out          = withApiFoodValues(beef, v);
    EXPECT_EQ(out.hunger, -MAX_API_FOOD_VALUE);
    EXPECT_FLOAT_EQ(out.saturation, static_cast<float>(MAX_API_FOOD_VALUE));
    // The hunger preview math stays defined for the clamped extremes.
    EXPECT_EQ(hungerPreviewIcons(20, MAX_API_FOOD_VALUE).last, 10);
}

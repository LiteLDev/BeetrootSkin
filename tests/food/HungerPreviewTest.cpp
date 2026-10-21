#include "core/food/HungerPreview.h"

#include <gtest/gtest.h>

using namespace beetroot::core;

namespace {
FoodState withHunger(int hunger) {
    FoodState s;
    s.hunger = hunger;
    return s;
}
} // namespace

TEST(HungerPreview, ShownOnlyWhenEnabledEdibleAndSurvival) {
    auto beef  = FoodValues::make(8, 0.8f);
    auto apple = FoodValues::make(4, 1.2f, true);
    EXPECT_TRUE(shouldShowHungerPreview(true, withHunger(8), beef));
    EXPECT_FALSE(shouldShowHungerPreview(false, withHunger(8), beef));
    // Full hunger: only always-edible food.
    EXPECT_FALSE(shouldShowHungerPreview(true, withHunger(20), beef));
    EXPECT_TRUE(shouldShowHungerPreview(true, withHunger(20), apple));
    // Creative / spectator: never (AppleSkin: !canHurtPlayer).
    auto creative         = withHunger(8);
    creative.creativeLike = true;
    EXPECT_FALSE(shouldShowHungerPreview(true, creative, beef));
    EXPECT_FALSE(shouldShowHungerPreview(true, creative, apple));
    // Food without nutrition.
    EXPECT_FALSE(shouldShowHungerPreview(true, withHunger(8), FoodValues::make(0, 0.f, true)));
}

TEST(HungerPreview, IconListMatchesAppleSkinRange) {
    // hunger 7 + 8 = 15: icons 3..7, the last one (index 7, hunger 15) is half.
    auto icons = hungerPreviewIconList(7, FoodValues::make(8, 0.8f));
    ASSERT_EQ(icons.size(), 5u);
    EXPECT_EQ(icons.front().index, 3);
    EXPECT_EQ(icons.back().index, 7);
    for (std::size_t i = 0; i + 1 < icons.size(); ++i) EXPECT_EQ(icons[i].sprite, ShankSprite::Full);
    EXPECT_EQ(icons.back().sprite, ShankSprite::Half);
    for (auto const& ic : icons) EXPECT_FALSE(ic.rotten);
}

TEST(HungerPreview, IconListEvenTotalHasNoHalf) {
    // hunger 6 + 4 = 10: icons 3..4, both full.
    auto icons = hungerPreviewIconList(6, FoodValues::make(4, 1.2f));
    ASSERT_EQ(icons.size(), 2u);
    EXPECT_EQ(icons[0], (HungerPreviewIcon{3, ShankSprite::Full, false}));
    EXPECT_EQ(icons[1], (HungerPreviewIcon{4, ShankSprite::Full, false}));
}

TEST(HungerPreview, IconListClampsAtTwenty) {
    // hunger 19 + 4 -> 20: only icon 9 (hunger 19-20), full.
    auto icons = hungerPreviewIconList(19, FoodValues::make(4, 1.2f, true));
    ASSERT_EQ(icons.size(), 1u);
    EXPECT_EQ(icons[0], (HungerPreviewIcon{9, ShankSprite::Full, false}));
    // Already full: the range is 20/2 = 10 .. ceil(20/2) = 10, so nothing is drawn (AppleSkin
    // shows only the saturation gain of an always-edible food then).
    EXPECT_TRUE(hungerPreviewIconList(20, FoodValues::make(4, 1.2f, true)).empty());
}

TEST(HungerPreview, RottenFoodUsesEffectSprites) {
    auto flesh   = FoodValues::make(4, 0.1f);
    flesh.rotten = true;
    // hunger 7 + 4 = 11: icons 3..5, the last one half.
    auto icons = hungerPreviewIconList(7, flesh);
    ASSERT_EQ(icons.size(), 3u);
    EXPECT_EQ(icons[0], (HungerPreviewIcon{3, ShankSprite::EffectFull, true}));
    EXPECT_EQ(icons[1], (HungerPreviewIcon{4, ShankSprite::EffectFull, true}));
    EXPECT_EQ(icons[2], (HungerPreviewIcon{5, ShankSprite::EffectHalf, true}));
}

TEST(HungerPreview, NoNutritionNoIcons) { EXPECT_TRUE(hungerPreviewIconList(5, FoodValues::make(0, 0.f)).empty()); }

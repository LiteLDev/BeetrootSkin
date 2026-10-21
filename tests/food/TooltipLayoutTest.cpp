#include "core/food/TooltipLayout.h"
#include "core/food/FoodMath.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <limits>

using namespace beetroot::core;
using namespace beetroot::core::tooltip;

namespace {
FoodTooltipLayout layoutOf(int hunger, float saturation) {
    return layoutFoodTooltip(hunger, saturation, hunger, saturation);
}
} // namespace

TEST(TooltipLayout, OutlineMatchesAppleSkin) {
    EXPECT_EQ(outlineFor(-2, -2, 0), Outline::Negative);
    EXPECT_EQ(outlineFor(4, 4, 0), Outline::Normal); // default == modified
    EXPECT_EQ(outlineFor(4, 4, 2), Outline::Normal);
    // Modified bigger than default: bars past the default value are EXTRA.
    EXPECT_EQ(outlineFor(6, 2, 0), Outline::Normal);
    EXPECT_EQ(outlineFor(6, 2, 2), Outline::Extra);
    EXPECT_EQ(outlineFor(6, 2, 4), Outline::Extra);
    // Modified smaller than default: partial where it ends, missing after.
    EXPECT_EQ(outlineFor(3, 6, 0), Outline::Normal);
    EXPECT_EQ(outlineFor(3, 6, 2), Outline::Partial);
    EXPECT_EQ(outlineFor(3, 6, 4), Outline::Missing);
    EXPECT_EQ(outlineFor(0, 6, 0), Outline::Missing);
}

TEST(TooltipLayout, OutlineColors) {
    EXPECT_EQ(outlineColor(Outline::Negative), (Rgba{1, 1, 1, 1}));
    EXPECT_EQ(outlineColor(Outline::Extra), (Rgba{0.06f, 0.32f, 0.02f, 1}));
    EXPECT_EQ(outlineColor(Outline::Partial), (Rgba{0.53f, 0.21f, 0.08f, 1}));
    EXPECT_EQ(outlineColor(Outline::Missing), (Rgba{0.62f, 0, 0, 0.5f}));
}

TEST(TooltipLayout, GoldenApple) {
    // 4 hunger, saturation gain 4 * 1.2 * 2 = 9.6.
    auto l = layoutOf(4, FoodValues::saturationFor(4, 1.2f));
    ASSERT_TRUE(l.visible());
    EXPECT_EQ(l.hungerBars, 2);
    EXPECT_TRUE(l.hungerBarsText.empty());
    EXPECT_EQ(l.saturationBars, 5);
    EXPECT_TRUE(l.saturationBarsText.empty());
    EXPECT_EQ(l.width(), 5 * 7 + 2);
    EXPECT_EQ(FoodTooltipLayout::height(), 20);

    ASSERT_EQ(l.hunger.size(), 2u);
    EXPECT_EQ(l.hunger[0].x, 9); // right to left
    EXPECT_EQ(l.hunger[1].x, 0);
    for (auto const& h : l.hunger) {
        EXPECT_EQ(h.outline, Outline::Normal);
        EXPECT_TRUE(h.showModified);
        EXPECT_FALSE(h.modifiedHalf);
        EXPECT_FALSE(h.defaultHalf);
    }

    ASSERT_EQ(l.saturation.size(), 5u);
    EXPECT_EQ(l.saturation[0].x, 28);
    EXPECT_EQ(l.saturation[4].x, 0);
    // eff = (9.6 - i) / 2: 4.8, 3.8, 2.8, 1.8, 0.8
    for (int k = 0; k < 4; ++k) EXPECT_EQ(l.saturation[k].u, 21);
    EXPECT_EQ(l.saturation[4].u, 14);
    for (auto const& s : l.saturation) {
        EXPECT_EQ(s.v, 27);
        EXPECT_FALSE(s.faded);
    }
    EXPECT_TRUE(l.hungerText.text.empty());
    EXPECT_TRUE(l.saturationText.text.empty());
}

TEST(TooltipLayout, HalfShankAndSmallSaturation) {
    // Dried kelp: 1 hunger, modifier 0.6 -> saturation 1.2.
    auto l = layoutOf(1, FoodValues::saturationFor(1, 0.6f));
    EXPECT_EQ(l.hungerBars, 1);
    ASSERT_EQ(l.hunger.size(), 1u);
    EXPECT_TRUE(l.hunger[0].defaultHalf);
    EXPECT_TRUE(l.hunger[0].modifiedHalf);
    EXPECT_TRUE(l.hunger[0].showModified);
    ASSERT_EQ(l.saturation.size(), 1u);
    EXPECT_EQ(l.saturation[0].u, 14); // eff 0.6
    // Rotten flesh: 4 hunger, saturation 0.8 -> one icon, eff 0.4.
    l = layoutOf(4, FoodValues::saturationFor(4, 0.1f));
    ASSERT_EQ(l.saturation.size(), 1u);
    EXPECT_EQ(l.saturation[0].u, 7);
    EXPECT_EQ(l.width(), 2 * 9 + 2);
}

TEST(TooltipLayout, SaturationIconsFaded) {
    // Default saturation bigger than modified: the extra icons are faded.
    auto l = layoutFoodTooltip(2, 6.f, 2, 2.f);
    EXPECT_EQ(l.saturationBars, 3);
    ASSERT_EQ(l.saturation.size(), 3u);
    EXPECT_FALSE(l.saturation[0].faded); // i = 0
    EXPECT_TRUE(l.saturation[1].faded);  // i = 2, |2| <= 2
    EXPECT_TRUE(l.saturation[2].faded);
    EXPECT_EQ(l.saturation[1].u, 28); // eff 0 -> empty outline
}

TEST(TooltipLayout, ZeroSaturationShowsX0) {
    auto l = layoutOf(2, 0.f);
    EXPECT_EQ(l.saturationBars, 1);
    EXPECT_EQ(l.saturationBarsText, "x0");
    ASSERT_EQ(l.saturation.size(), 1u);
    EXPECT_EQ(l.saturation[0].u, 28);
    EXPECT_TRUE(l.saturation[0].faded);
    EXPECT_EQ(l.saturationText.text, "x0");
    EXPECT_FLOAT_EQ(l.saturationText.x, 7 + 1.5f);
    EXPECT_FLOAT_EQ(l.saturationText.y, 10 + 0.75f);
    EXPECT_EQ(l.width(), std::max(9, 7 + 12) + 2);
}

TEST(TooltipLayout, OverflowHunger) {
    auto l = layoutOf(30, 40.f);
    EXPECT_EQ(l.hungerBars, 1);
    EXPECT_EQ(l.hungerBarsText, "x15");
    EXPECT_EQ(l.saturationBars, 1);
    EXPECT_EQ(l.saturationBarsText, "x20");
    ASSERT_EQ(l.hunger.size(), 1u);
    EXPECT_EQ(l.hunger[0].x, 0);
    EXPECT_FALSE(l.hunger[0].defaultHalf);
    EXPECT_EQ(l.hungerText.text, "x15");
    EXPECT_FLOAT_EQ(l.hungerText.x, 9 + 1.5f);
    EXPECT_FLOAT_EQ(l.hungerText.y, 1.5f);
    EXPECT_EQ(l.width(), 9 + 18 + 2);
    // Exactly 10 bars still draws icons.
    l = layoutOf(20, 20.f);
    EXPECT_EQ(l.hungerBars, 10);
    EXPECT_TRUE(l.hungerBarsText.empty());
    EXPECT_EQ(l.saturationBars, 10);
}

TEST(TooltipLayout, NegativeValues) {
    auto l = layoutOf(-4, -3.f);
    EXPECT_EQ(l.hungerBars, 2);
    ASSERT_EQ(l.hunger.size(), 2u);
    EXPECT_EQ(l.hunger[0].outline, Outline::Negative);
    EXPECT_FALSE(l.hunger[0].showModified);
    EXPECT_EQ(l.saturationBars, 2);
    ASSERT_EQ(l.saturation.size(), 2u);
    EXPECT_EQ(l.saturation[0].v, 34);
    EXPECT_EQ(l.saturation[0].u, 21); // eff 1.5
    EXPECT_EQ(l.saturation[1].u, 7);  // eff 0.5 is not > 0.5
}

TEST(TooltipLayout, NegativeOverflowText) {
    auto l = layoutOf(-22, -1.f);
    EXPECT_EQ(l.hungerBarsText, "x-11");
}

TEST(TooltipLayout, ExtremeValuesAreClamped) {
    auto l = layoutFoodTooltip(std::numeric_limits<int>::max(), 1e30f, std::numeric_limits<int>::min(), -1e30f);
    EXPECT_EQ(l.hungerBars, 1);
    EXPECT_EQ(l.hungerBarsText, "x500000");
    EXPECT_EQ(l.saturationBarsText, "x500000");
    ASSERT_EQ(l.hunger.size(), 1u);
    EXPECT_EQ(l.hunger[0].outline, Outline::Negative);
}

TEST(TooltipLayout, NothingToShow) {
    auto l = layoutOf(0, 0.f);
    EXPECT_FALSE(l.visible());
    EXPECT_TRUE(l.hunger.empty());
    l = layoutOf(0, std::numeric_limits<float>::quiet_NaN());
    EXPECT_FALSE(l.visible());
}

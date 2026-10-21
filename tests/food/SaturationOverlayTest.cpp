#include "core/food/SaturationOverlay.h"

#include <gtest/gtest.h>

#include <limits>
#include <string>

using namespace beetroot::core;

namespace {
FoodState withFood(int hunger, float saturation) {
    FoodState s;
    s.hunger     = hunger;
    s.saturation = saturation;
    return s;
}

/// "index:u index:u ..." for compact expectations.
std::string describe(std::vector<SaturationIcon> const& icons) {
    std::string out;
    for (auto const& i : icons) {
        if (!out.empty()) out += ' ';
        out += std::to_string(i.index) + ':' + std::to_string(i.u);
    }
    return out;
}
} // namespace

TEST(SaturationOverlay, BaseOverlayFullAndPartialIcons) {
    // 5 saturation: two full icons and a half (0.5 -> u 9) third icon.
    EXPECT_EQ(describe(saturationOverlaySprites(5, 0)), "0:27 1:27 2:9");
    // 1.5 -> 0.75 of the first icon -> three-quarter sprite.
    EXPECT_EQ(describe(saturationOverlaySprites(1.5f, 0)), "0:18");
    // 0.4 -> 0.2 of an icon -> quarter sprite (u 0).
    EXPECT_EQ(describe(saturationOverlaySprites(0.4f, 0)), "0:0");
    EXPECT_TRUE(saturationOverlaySprites(0, 0).empty());
}

TEST(SaturationOverlay, FullBarIsTenFullIcons) {
    auto icons = saturationOverlaySprites(20, 0);
    ASSERT_EQ(icons.size(), 10u);
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(icons[i].index, i);
        EXPECT_EQ(icons[i].u, 27);
    }
    // Over-saturated values are clamped to 20.
    EXPECT_EQ(saturationOverlaySprites(25, 0).size(), 10u);
}

TEST(SaturationOverlay, GainStartsAtCurrentTail) {
    // 5 + 4 = 9: icons 2..4, icon 2 now full, icon 4 half.
    EXPECT_EQ(describe(saturationOverlaySprites(5, 4)), "2:27 3:27 4:9");
    // 4 + 2 = 6: tail starts at icon 2 (empty before), full.
    EXPECT_EQ(describe(saturationOverlaySprites(4, 2)), "2:27");
    // Gains past 20 are clamped.
    EXPECT_EQ(describe(saturationOverlaySprites(18, 10)), "9:27");
}

TEST(SaturationOverlay, CappedGainDoesNotGrowAnExtraIcon) {
    // Steak at hunger 2 / saturation 0.1: capped at 10, gain = 10 - 0.1f. The float sum must not
    // spill into a sixth icon.
    FoodState s;
    s.hunger     = 2;
    s.saturation = 0.1f;
    float gain   = saturationPreviewGain(s, FoodValues::make(8, 0.8f));
    EXPECT_EQ(describe(saturationOverlaySprites(0.1f, gain)), "0:27 1:27 2:27 3:27 4:27");
    EXPECT_EQ(describe(saturationOverlaySprites(0.3f, 10.f - 0.3f)), "0:27 1:27 2:27 3:27 4:27");
}

TEST(SaturationOverlay, InvalidInputs) {
    EXPECT_TRUE(saturationOverlaySprites(1, -5).empty());
    EXPECT_TRUE(saturationOverlaySprites(std::numeric_limits<float>::quiet_NaN(), 0).empty());
    EXPECT_TRUE(saturationOverlaySprites(5, std::numeric_limits<float>::infinity()).empty());
}

TEST(SaturationOverlay, PreviewGainCappedByNewHunger) {
    // Steak (8, 0.8 -> 12.8 saturation) at hunger 6 / saturation 0: new hunger 14 >= 12.8.
    auto steak = FoodValues::make(8, 0.8f);
    EXPECT_FLOAT_EQ(saturationPreviewGain(withFood(6, 0), steak), 12.8f);
    // ... at hunger 2: capped at the new hunger level 10.
    EXPECT_FLOAT_EQ(saturationPreviewGain(withFood(2, 0), steak), 10.f);
    EXPECT_FLOAT_EQ(saturationPreviewGain(withFood(10, 5), steak), 12.8f);
    EXPECT_FLOAT_EQ(saturationPreviewGain(withFood(18, 15), steak), 5.f);
}

TEST(SaturationOverlay, PreviewGainZeroWhenNotEdible) {
    auto steak = FoodValues::make(8, 0.8f);
    EXPECT_FLOAT_EQ(saturationPreviewGain(withFood(20, 5), steak), 0.f);
    auto apple = FoodValues::make(4, 1.2f, true); // golden apple: can always eat
    EXPECT_FLOAT_EQ(saturationPreviewGain(withFood(20, 5), apple), 9.6f);
    // Saturation already above the new hunger level: nothing gained.
    EXPECT_FLOAT_EQ(saturationPreviewGain(withFood(4, 10), FoodValues::make(1, 0.1f)), 0.f);
}

TEST(SaturationOverlay, PeacefulFullBarStillPreviewsSaturation) {
    // Bedrock lets Peaceful players eat with a full bar, so the saturation gain is previewed.
    auto steak = FoodValues::make(8, 0.8f);
    auto s     = withFood(20, 5);
    s.peaceful = true;
    EXPECT_FLOAT_EQ(saturationPreviewGain(s, steak), 12.8f);
}

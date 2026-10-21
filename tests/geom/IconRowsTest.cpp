#include "core/geom/IconRows.h"

#include <gtest/gtest.h>

#include <vector>

using namespace beetroot::core;

namespace {
std::vector<float> row(float y, int n = 10) { return std::vector<float>(static_cast<std::size_t>(n), y); }
} // namespace

TEST(RestingRows, AllIconsShakingUsesLearnedLine) {
    RestingRows rows;
    // A still frame teaches the resting line...
    EXPECT_EQ(rows.update(row(0), 10), row(0));
    EXPECT_TRUE(rows.learned(0));
    // ...so a frame in which every icon shakes (none at 0) keeps it: offsets are -1/+1.
    std::vector<float> shaking{-1, 1, 1, -1, 1, -1, -1, 1, 1, -1};
    auto               rest = rows.update(shaking, 10);
    EXPECT_EQ(rest, row(0));
    EXPECT_EQ(jitterOffsets(shaking, rest), shaking);
}

TEST(RestingRows, LockstepShakeCannotOutvoteRest) {
    RestingRows rows;
    for (int i = 0; i < 5; ++i) (void)rows.update(row(0), 10);
    // All ten hearts happen to shake down together for a few frames: looks still, but is outvoted.
    for (int i = 0; i < 3; ++i) EXPECT_EQ(rows.update(row(1), 10), row(0));
    // A layout that really moved wins once it has been seen more often.
    for (int i = 0; i < 5; ++i) (void)rows.update(row(4), 10);
    EXPECT_EQ(rows.update(row(4), 10), row(4));
}

TEST(RestingRows, FallbackBeforeAnyStillFrame) {
    std::vector<float> shaking{-1, 1, 0, -1, 1, 0, 0, 1, 1, 1};
    RestingRows        a;
    EXPECT_EQ(a.update(shaking, 10, 0.f), row(0)); // the given row-0 line
    EXPECT_FALSE(a.learned(0));
    RestingRows b;
    EXPECT_EQ(b.update(shaking, 10), row(1)); // most common y of the row
    RestingRows c;
    EXPECT_EQ(c.update(shaking, 10, 9.f), row(1)); // implausible fallback (bar rests elsewhere)
    // Regeneration-like wave: one icon raised by 2, the rest stays.
    std::vector<float> wave = row(-10);
    wave[3]                 = -12;
    RestingRows d;
    EXPECT_EQ(d.update(wave, 10), row(-10));
}

TEST(RestingRows, MultipleRowsAndSingleIconTopRow) {
    RestingRows rows;
    // 11 hearts: a full bottom row at 0 and one heart on the row above at -10.
    std::vector<float> still = row(0);
    still.push_back(-10);
    EXPECT_EQ(rows.update(still, 10), still);
    EXPECT_TRUE(rows.learned(1));
    // Low health: every heart shakes down by 0..1, including the lone top heart.
    std::vector<float> shaking{1, 0, 1, 1, 0, 1, 1, 1, 0, 1, -9};
    auto               rest = rows.update(shaking, 10);
    EXPECT_EQ(rest, still);
    auto off = jitterOffsets(shaking, rest);
    EXPECT_FLOAT_EQ(off[0], 1);
    EXPECT_FLOAT_EQ(off[1], 0);
    EXPECT_FLOAT_EQ(off[10], 1);
}

TEST(RestingRows, ShakingFrameDoesNotVote) {
    RestingRows rows;
    (void)rows.update(row(5, 12), 10);
    // One row level, the other not: not a still frame.
    std::vector<float> ys = row(7);
    ys.insert(ys.end(), {-3, -4});
    for (int i = 0; i < 5; ++i) {
        auto const& rest = rows.update(ys, 10);
        EXPECT_FLOAT_EQ(rest[0], 5);
        EXPECT_FLOAT_EQ(rest[11], 5);
    }
}

TEST(RestingRows, LayoutChangeAndResetForget) {
    RestingRows rows;
    (void)rows.update(row(3), 10);
    EXPECT_TRUE(rows.learned(0));
    EXPECT_EQ(rows.update(row(6, 9), 10), row(6, 9)); // count changed: relearned
    rows.reset();
    EXPECT_FALSE(rows.learned(0));
    EXPECT_EQ(rows.update(row(2, 9), 10), row(2, 9));
    EXPECT_TRUE(rows.update({}, 10).empty());
    EXPECT_EQ(rows.update({4}, 10, 3.f), (std::vector<float>{3})); // 1 icon: never still, uses fallback
    EXPECT_FALSE(rows.learned(0));
    EXPECT_EQ(rows.update({4, 4}, 0), (std::vector<float>{4, 4})); // perRow 0: one row
}

TEST(RestingRows, JitterOffsetsSizes) {
    EXPECT_EQ(jitterOffsets({1, 2, 3}, {1, 1}), (std::vector<float>{0, 1}));
    EXPECT_TRUE(jitterOffsets({}, {1}).empty());
}

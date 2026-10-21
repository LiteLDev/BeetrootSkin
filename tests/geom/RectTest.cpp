#include "core/geom/Rect.h"

#include <gtest/gtest.h>

using beetroot::core::Rect;

TEST(Rect, Basics) {
    Rect r{10, 20, 30, 40};
    EXPECT_FLOAT_EQ(r.right(), 40);
    EXPECT_FLOAT_EQ(r.bottom(), 60);
    EXPECT_TRUE(r.contains(10, 20));
    EXPECT_FALSE(r.contains(40, 20));
    EXPECT_FALSE(r.empty());
    EXPECT_TRUE((Rect{0, 0, 0, 5}).empty());
    EXPECT_EQ(r.translated(1, 2), (Rect{11, 22, 30, 40}));
    EXPECT_EQ(r.inset(5), (Rect{15, 25, 20, 30}));
    EXPECT_EQ(r.inset(100), (Rect{25, 40, 0, 0}));
}

TEST(Rect, United) {
    Rect a{0, 0, 10, 10}, b{20, 5, 5, 10};
    EXPECT_EQ(a.united(b), (Rect{0, 0, 25, 15}));
    EXPECT_EQ(a.united(Rect{}), a);
    EXPECT_EQ(Rect{}.united(b), b);
}

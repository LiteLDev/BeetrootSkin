#include "core/state/Config.h"
#include "core/state/AtomicWrite.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>

using namespace beetroot::core;

TEST(Config, Defaults) {
    Config c;
    EXPECT_EQ(c.version, Config::CURRENT_VERSION);
    EXPECT_TRUE(c.showFoodValuesInTooltip);
    EXPECT_TRUE(c.showFoodValuesInTooltipAlways);
    EXPECT_TRUE(c.showSaturationHudOverlay);
    EXPECT_TRUE(c.showFoodValuesHudOverlay);
    EXPECT_TRUE(c.showFoodValuesHudOverlayWhenOffhand);
    EXPECT_TRUE(c.showFoodExhaustionHudUnderlay);
    EXPECT_TRUE(c.showFoodHealthHudOverlay);
    EXPECT_TRUE(c.showVanillaAnimationsOverlay);
    EXPECT_FLOAT_EQ(c.maxHudOverlayFlashAlpha, 0.65f);
    EXPECT_FALSE(c.showFoodDebugInfo);
}

TEST(Config, RoundTrip) {
    Config c;
    c.showFoodValuesInTooltip       = false;
    c.showFoodExhaustionHudUnderlay = false;
    c.showFoodDebugInfo             = true;
    c.maxHudOverlayFlashAlpha       = 0.3f;
    Config d;
    ASSERT_TRUE(d.fromJsonText(c.toJsonText()));
    EXPECT_EQ(c, d);
}

TEST(Config, CorruptOrWrongTypeKeepsDefaults) {
    Config c;
    EXPECT_FALSE(c.fromJsonText("{not json"));
    EXPECT_EQ(c, Config{});
    EXPECT_FALSE(c.fromJsonText("[1,2]"));
    EXPECT_TRUE(
        c.fromJsonText(R"({"showFoodDebugInfo": "yes", "maxHudOverlayFlashAlpha": "x", "showFoodValuesInTooltip": 0})")
    );
    EXPECT_FALSE(c.showFoodDebugInfo);
    EXPECT_TRUE(c.showFoodValuesInTooltip);
    EXPECT_FLOAT_EQ(c.maxHudOverlayFlashAlpha, 0.65f);
}

TEST(Config, ClampsAlpha) {
    Config c;
    ASSERT_TRUE(c.fromJsonText(R"({"maxHudOverlayFlashAlpha": 3})"));
    EXPECT_FLOAT_EQ(c.maxHudOverlayFlashAlpha, 1.f);
    ASSERT_TRUE(c.fromJsonText(R"({"maxHudOverlayFlashAlpha": -0.5})"));
    EXPECT_FLOAT_EQ(c.maxHudOverlayFlashAlpha, 0.f);
    c.maxHudOverlayFlashAlpha = 7;
    auto j                    = nlohmann::json::parse(c.toJsonText());
    EXPECT_FLOAT_EQ(j["maxHudOverlayFlashAlpha"].get<float>(), 1.f);
}

TEST(Config, MissingVersionMigratesAndUnknownFieldsSurvive) {
    Config c;
    ASSERT_TRUE(c.fromJsonText(R"({"showFoodHealthHudOverlay": false, "futureThing": {"a": 1}, "version": 7})"));
    EXPECT_FALSE(c.showFoodHealthHudOverlay);
    EXPECT_EQ(c.version, 7);
    auto j = nlohmann::json::parse(c.toJsonText());
    EXPECT_EQ(j["futureThing"]["a"], 1);
    EXPECT_EQ(j["version"], 7);
    ASSERT_TRUE(c.fromJsonText(R"({})"));
    EXPECT_EQ(c.version, Config::CURRENT_VERSION);
    EXPECT_TRUE(c.extraFields.empty());
}

TEST(Config, SaveLoadFile) {
    auto dir  = std::filesystem::temp_directory_path() / "beetroot-tests";
    auto file = dir / "config" / "config.json";
    std::filesystem::remove_all(dir);
    Config c;
    EXPECT_FALSE(c.load(file));
    EXPECT_EQ(c, Config{});
    c.showVanillaAnimationsOverlay = false;
    ASSERT_TRUE(c.save(file));
    Config d;
    ASSERT_TRUE(d.load(file));
    EXPECT_EQ(c, d);
    EXPECT_FALSE(std::filesystem::exists(file.string() + ".tmp"));
    std::filesystem::remove_all(dir);
}

TEST(AtomicWrite, ReadMissingFile) {
    EXPECT_FALSE(readFile(std::filesystem::temp_directory_path() / "beetroot-definitely-missing.json").has_value());
}

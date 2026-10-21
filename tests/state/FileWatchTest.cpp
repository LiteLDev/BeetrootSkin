#include "core/state/FileWatch.h"
#include "core/state/AtomicWrite.h"
#include "core/state/Config.h"

#include <gtest/gtest.h>

#include <filesystem>

using namespace beetroot::core;
using namespace std::chrono_literals;

namespace {

using Clock = FileChangeDetector::Clock;

FileStamp stamp(std::int64_t mtime, std::uint64_t size) { return {true, mtime, size}; }

} // namespace

TEST(FileChangeDetector, DueOncePerInterval) {
    FileChangeDetector d(1000ms, 250ms);
    auto               t0 = Clock::time_point{} + 10s;
    EXPECT_TRUE(d.due(t0)); // never polled
    d.acknowledge(stamp(1, 10), t0);
    EXPECT_FALSE(d.due(t0 + 999ms));
    EXPECT_TRUE(d.due(t0 + 1000ms));
    EXPECT_FALSE(d.update(stamp(1, 10), t0 + 1000ms)); // unchanged
    EXPECT_FALSE(d.due(t0 + 1500ms));
    EXPECT_TRUE(d.due(t0 + 2000ms));
}

TEST(FileChangeDetector, ChangeIsReportedOnceAfterSettling) {
    FileChangeDetector d(1000ms, 250ms);
    auto               t = Clock::time_point{} + 10s;
    d.acknowledge(stamp(1, 10), t);
    t += 1s;
    EXPECT_FALSE(d.update(stamp(2, 12), t)); // first seen: wait for it to settle
    EXPECT_TRUE(d.pending());
    EXPECT_FALSE(d.due(t + 200ms));
    EXPECT_TRUE(d.due(t + 250ms)); // re-check after the settle delay, not a full interval
    t += 250ms;
    EXPECT_TRUE(d.update(stamp(2, 12), t)); // stable: apply
    EXPECT_FALSE(d.pending());
    EXPECT_EQ(d.known(), stamp(2, 12));
    t += 1s;
    EXPECT_FALSE(d.update(stamp(2, 12), t)); // the same change is never reported twice
}

TEST(FileChangeDetector, StillBeingWrittenWaitsAgain) {
    FileChangeDetector d(1000ms, 250ms);
    auto               t = Clock::time_point{} + 10s;
    d.acknowledge(stamp(1, 10), t);
    t += 1s;
    EXPECT_FALSE(d.update(stamp(2, 5), t)); // half-written
    t += 250ms;
    EXPECT_FALSE(d.update(stamp(3, 12), t)); // grew meanwhile: new candidate
    t += 250ms;
    EXPECT_TRUE(d.update(stamp(3, 12), t));
}

TEST(FileChangeDetector, RevertedBeforeSettlingIsNotAChange) {
    FileChangeDetector d(1000ms, 250ms);
    auto               t = Clock::time_point{} + 10s;
    d.acknowledge(stamp(1, 10), t);
    t += 1s;
    EXPECT_FALSE(d.update(stamp(2, 10), t));
    t += 250ms;
    EXPECT_FALSE(d.update(stamp(1, 10), t));
    EXPECT_FALSE(d.pending());
}

TEST(FileChangeDetector, OwnWriteIsNeverReported) {
    FileChangeDetector d(1000ms, 250ms);
    auto               t = Clock::time_point{} + 10s;
    d.acknowledge(stamp(1, 10), t);
    t += 1s;
    EXPECT_FALSE(d.update(stamp(2, 11), t)); // the user's edit is pending...
    d.acknowledge(stamp(3, 11), t + 10ms);   // ...and we save over it
    EXPECT_FALSE(d.pending());
    for (int i = 0; i < 5; ++i) {
        t += 1s;
        EXPECT_FALSE(d.update(stamp(3, 11), t));
    }
}

TEST(FileChangeDetector, DeletionAndRecreationAreChanges) {
    FileChangeDetector d(1000ms, 250ms);
    auto               t = Clock::time_point{} + 10s;
    d.acknowledge(stamp(1, 10), t);
    t += 1s;
    EXPECT_FALSE(d.update(FileStamp{}, t));
    t += 250ms;
    EXPECT_TRUE(d.update(FileStamp{}, t));
    t += 1s;
    EXPECT_FALSE(d.update(stamp(4, 10), t));
    t += 250ms;
    EXPECT_TRUE(d.update(stamp(4, 10), t));
}

TEST(FileStamp, StatsRealFiles) {
    auto dir  = std::filesystem::temp_directory_path() / "beetroot-tests-filewatch";
    auto file = dir / "config.json";
    std::filesystem::remove_all(dir);
    EXPECT_EQ(statFile(file), FileStamp{});
    ASSERT_TRUE(writeFileAtomic(file, "{}"));
    auto a = statFile(file);
    EXPECT_TRUE(a.exists);
    EXPECT_EQ(a.size, 2u);
    EXPECT_EQ(statFile(file), a); // stable
    ASSERT_TRUE(writeFileAtomic(file, R"({"showFoodDebugInfo": true})"));
    EXPECT_NE(statFile(file), a);
    EXPECT_EQ(statFile(dir), FileStamp{}); // a directory is not a config file
    std::filesystem::remove_all(dir);
}

TEST(ConfigReload, ChangedFieldsListsEveryDifference) {
    Config a;
    Config b = a;
    EXPECT_TRUE(changedFields(a, b).empty());
    b.showSaturationHudOverlay       = false;
    b.showFoodExhaustionHudUnderlay  = false;
    b.maxHudOverlayFlashAlpha        = 0.25f;
    b.showFoodDebugInfo              = true;
    auto                     changes = changedFields(a, b);
    std::vector<std::string> expected{
        "showSaturationHudOverlay=false",
        "showFoodExhaustionHudUnderlay=false",
        "showFoodDebugInfo=true",
        "maxHudOverlayFlashAlpha=0.25",
    };
    EXPECT_EQ(changes, expected);
    b             = a;
    b.extraFields = R"({"x":1})";
    b.version     = 3;
    expected      = {"version=3", "extraFields"};
    EXPECT_EQ(changedFields(a, b), expected);
}

TEST(ConfigReload, FloatIsPrintedShortest) {
    Config a;
    Config b;
    b.maxHudOverlayFlashAlpha = 0.3f;
    EXPECT_EQ(changedFields(a, b), std::vector<std::string>{"maxHudOverlayFlashAlpha=0.3"});
}

TEST(ConfigReload, ParseErrorIsReported) {
    for (char const* bad : {"", "{not json", R"({"showFoodDebugInfo": true,})", "[1,2]", "42", "null"}) {
        Config      c;
        std::string err;
        EXPECT_FALSE(c.fromJsonText(bad, err)) << bad;
        EXPECT_FALSE(err.empty()) << bad;
        EXPECT_EQ(c, Config{}) << bad;
    }
    for (char const* good : {"{}", R"({"showFoodDebugInfo": "wrong type"})", "\xEF\xBB\xBF{\"version\": 1}"}) {
        Config      c;
        std::string err = "stale";
        EXPECT_TRUE(c.fromJsonText(good, err)) << good;
        EXPECT_TRUE(err.empty()) << good;
    }
    Config      c;
    std::string err;
    EXPECT_FALSE(c.fromJsonText("{\n  \"a\": 1 \"b\": 2\n}", err)); // missing comma on line 2
    EXPECT_NE(err.find("line 2"), std::string::npos) << err;
    EXPECT_FALSE(c.fromJsonText("[]", err));
    EXPECT_EQ(err, "not a JSON object");
}

TEST(ConfigReload, MergeKeepsBothSidesChanges) {
    Config base;                            // what the file held when last read
    Config ours                     = base; // in memory: F6 toggled the debug line
    ours.showFoodDebugInfo          = true;
    Config theirs                   = base; // on disk: the user's unread edit
    theirs.showSaturationHudOverlay = false;
    theirs.maxHudOverlayFlashAlpha  = 0.4f;
    theirs.extraFields              = R"({"future":1})";
    auto m                          = mergeConfig(base, ours, theirs);
    EXPECT_TRUE(m.showFoodDebugInfo);
    EXPECT_FALSE(m.showSaturationHudOverlay);
    EXPECT_FLOAT_EQ(m.maxHudOverlayFlashAlpha, 0.4f);
    EXPECT_EQ(m.extraFields, theirs.extraFields);
    // Both changed the same option: ours (the explicit in-game action) wins.
    theirs.showFoodDebugInfo     = false;
    ours.maxHudOverlayFlashAlpha = 0.9f;
    m                            = mergeConfig(base, ours, theirs);
    EXPECT_TRUE(m.showFoodDebugInfo);
    EXPECT_FLOAT_EQ(m.maxHudOverlayFlashAlpha, 0.9f);
    // Nothing changed in memory: the file wins entirely.
    EXPECT_EQ(mergeConfig(base, base, theirs), theirs);
}

TEST(ConfigReload, ReloadedFileRoundTripsEveryOption) {
    // The live reload path: an edited file parsed into a fresh Config equals the edit, and
    // saving it back writes the same settings (no spurious change on the next reload).
    Config edited;
    edited.showFoodValuesInTooltipAlways = false;
    edited.showSaturationHudOverlay      = false;
    edited.showFoodExhaustionHudUnderlay = false;
    edited.maxHudOverlayFlashAlpha       = 0.2f;
    Config reloaded;
    ASSERT_TRUE(reloaded.fromJsonText(edited.toJsonText()));
    EXPECT_EQ(reloaded, edited);
    EXPECT_TRUE(changedFields(edited, reloaded).empty());
    Config again;
    ASSERT_TRUE(again.fromJsonText(reloaded.toJsonText()));
    EXPECT_EQ(again, reloaded);
}

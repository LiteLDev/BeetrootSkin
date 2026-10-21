#include "app/Mod.h"

#include "mod/Log.h"

namespace beetroot::app {

namespace {
std::filesystem::path gModDir;
std::filesystem::path gConfigDir;
core::Config          gConfig;
bool                  gDebugDraw = false;

std::filesystem::path configFile() { return gConfigDir / "config.json"; }
} // namespace

std::filesystem::path const& modDir() { return gModDir; }
std::filesystem::path const& configDir() { return gConfigDir; }
std::filesystem::path        modFile(std::filesystem::path const& relative) { return gModDir / relative; }

core::Config& config() { return gConfig; }

bool saveConfig() {
    bool ok = gConfig.save(configFile());
    if (!ok) log().warn("Failed to save {}", configFile().string());
    return ok;
}

bool debugDraw() { return gDebugDraw; }

namespace detail {

void initPaths(std::filesystem::path const& modDir, std::filesystem::path const& configDir) {
    gModDir    = std::filesystem::absolute(modDir);
    gConfigDir = std::filesystem::absolute(configDir);
    std::error_code ec;
    gDebugDraw = std::filesystem::exists(gModDir / "debug_draw", ec);
}

void loadConfig() {
    std::error_code ec;
    bool            existed = std::filesystem::exists(configFile(), ec);
    if (!gConfig.load(configFile()) && existed) {
        // Keep the broken file for the user to fix; run with defaults.
        log().warn("Config {} is invalid; using defaults", configFile().string());
        return;
    }
    // Write back so new fields appear in the file with their defaults.
    saveConfig();
}

} // namespace detail

} // namespace beetroot::app

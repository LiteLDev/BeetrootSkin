#pragma once

// Process-wide mod state that every unit may read: paths and the user configuration.

#include "core/state/Config.h"

#include <filesystem>

namespace beetroot::app {

/// bin/BeetrootSkin as deployed (mods/BeetrootSkin). Valid after BeetrootSkin::load().
[[nodiscard]] std::filesystem::path const& modDir();
/// mods/BeetrootSkin/config.
[[nodiscard]] std::filesystem::path const& configDir();
/// Absolute path of a mod-owned file, e.g. modFile("textures/icons.png").
[[nodiscard]] std::filesystem::path modFile(std::filesystem::path const& relative);

/// The live configuration (client thread). Loaded in BeetrootSkin::load().
[[nodiscard]] core::Config& config();
/// Persist config() to configDir()/config.json (atomic). Returns false on I/O failure.
bool saveConfig();

/// True if `<modDir>/debug_draw` exists (checked once at load): enables the scaffold's debug
/// HUD layer and debug tooltip extension (src/debug/), used by the e2e smoke scenario.
[[nodiscard]] bool debugDraw();

namespace detail {
/// Called once by BeetrootSkin::load().
void initPaths(std::filesystem::path const& modDir, std::filesystem::path const& configDir);
/// Called once by BeetrootSkin::load(), after initPaths() and before loadTranslations(). Loads
/// the config, then runs app::loadModules() (every Module::onLoad()): src/mod can't be edited,
/// so this is the load-time hook point.
void loadConfig();
} // namespace detail

} // namespace beetroot::app

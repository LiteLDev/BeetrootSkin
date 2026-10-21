#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace beetroot::app {

/// Load bin/BeetrootSkin/lang/<locale>.json (merged at build time from assets/lang/<locale>/*.json).
void loadTranslations(std::filesystem::path const& langDir);

/// Translate a BeetrootSkin key ("beetroot.*") into the game's current language, falling back to en_US,
/// then to `fallback` (or the key itself when fallback is empty).
[[nodiscard]] std::string tr(std::string_view key, std::string_view fallback = {});

} // namespace beetroot::app

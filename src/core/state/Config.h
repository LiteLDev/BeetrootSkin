#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace beetroot::core {

/// User configuration, persisted as JSON in the mod's config dir (config/config.json).
/// Field names follow AppleSkin's config so its documentation applies.
///
/// Adding a field: add the member with its default, read/write it in Config.cpp
/// (fromJsonText/toJsonText + KNOWN_KEYS), clamp it in sanitize() if it has a range, and add a
/// test in tests/state/ConfigTest.cpp. Older files simply lack the key and get the default.
struct Config {
    /// Version written by this build. Older files are migrated on load (see Config.cpp `migrate`).
    static constexpr int CURRENT_VERSION = 1;

    int version = CURRENT_VERSION; ///< After load: max(file version, CURRENT_VERSION).

    bool  showFoodValuesInTooltip       = true; ///< Hunger/saturation icons in food tooltips (while Shift is held).
    bool  showFoodValuesInTooltipAlways = true; ///< ... even without Shift.
    bool  showSaturationHudOverlay      = true; ///< Saturation outline over the hunger bar.
    bool  showFoodValuesHudOverlay      = true; ///< Flashing hunger/saturation gain of the held food.
    bool  showFoodValuesHudOverlayWhenOffhand = true;  ///< ... also for food in the offhand.
    bool  showFoodExhaustionHudUnderlay       = true;  ///< Exhaustion bar under the hunger bar.
    bool  showFoodHealthHudOverlay            = true;  ///< Flashing health gain on the hearts.
    bool  showVanillaAnimationsOverlay        = true;  ///< Follow the vanilla hunger-bar jitter.
    float maxHudOverlayFlashAlpha             = 0.65f; ///< Peak alpha of the flashing overlays, [0,1].
    bool  showFoodDebugInfo                   = false; ///< Debug text with exact hunger/saturation/exhaustion.

    /// Fields found in the loaded file that this build doesn't know (a JSON object as text, or
    /// empty). Written back unchanged by save() so a newer build's settings survive a downgrade.
    std::string extraFields;

    /// Missing/corrupt file -> defaults (and returns false). Unknown fields are kept in
    /// extraFields; fields with the wrong type keep their defaults, out-of-range values are clamped.
    bool load(std::filesystem::path const& file);
    /// Atomic (temp file + rename). Returns false on I/O failure; the old file is then untouched.
    bool save(std::filesystem::path const& file) const;

    /// Same as load/save, on JSON text.
    bool                      fromJsonText(std::string_view text);
    [[nodiscard]] std::string toJsonText() const;

    /// Clamp numeric fields into their valid ranges.
    void sanitize();

    friend bool operator==(Config const&, Config const&) = default;
};

} // namespace beetroot::core

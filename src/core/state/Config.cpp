#include "core/state/Config.h"

#include "core/state/AtomicWrite.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <string_view>

namespace beetroot::core {

namespace {

using json = nlohmann::json;

struct BoolField {
    char const* key;
    bool Config::* member;
};

/// Every boolean option, in file order.
constexpr BoolField BOOL_FIELDS[] = {
    {"showFoodValuesInTooltip",             &Config::showFoodValuesInTooltip            },
    {"showFoodValuesInTooltipAlways",       &Config::showFoodValuesInTooltipAlways      },
    {"showSaturationHudOverlay",            &Config::showSaturationHudOverlay           },
    {"showFoodValuesHudOverlay",            &Config::showFoodValuesHudOverlay           },
    {"showFoodValuesHudOverlayWhenOffhand", &Config::showFoodValuesHudOverlayWhenOffhand},
    {"showFoodExhaustionHudUnderlay",       &Config::showFoodExhaustionHudUnderlay      },
    {"showFoodHealthHudOverlay",            &Config::showFoodHealthHudOverlay           },
    {"showVanillaAnimationsOverlay",        &Config::showVanillaAnimationsOverlay       },
    {"showFoodDebugInfo",                   &Config::showFoodDebugInfo                  },
};

/// Keys read by fromJsonText. Everything else is preserved in extraFields.
bool isKnownKey(std::string_view key) {
    if (key == "version" || key == "maxHudOverlayFlashAlpha") return true;
    return std::any_of(std::begin(BOOL_FIELDS), std::end(BOOL_FIELDS), [&](auto const& f) { return key == f.key; });
}

// Strictly typed readers: a value of the wrong JSON type leaves the field untouched.
void read(json const& j, char const* key, bool& field) {
    if (auto it = j.find(key); it != j.end() && it->is_boolean()) field = it->get<bool>();
}

void read(json const& j, char const* key, float& field) {
    if (auto it = j.find(key); it != j.end() && it->is_number()) {
        double d = it->get<double>();
        if (std::isfinite(d)) field = static_cast<float>(d);
    }
}

void read(json const& j, char const* key, int& field) {
    auto it = j.find(key);
    if (it == j.end() || !it->is_number()) return;
    double d = it->get<double>();
    if (std::isfinite(d) && d == std::floor(d) && d >= -1e9 && d <= 1e9) field = static_cast<int>(d);
}

/// Upgrade a config document from `fromVersion` to Config::CURRENT_VERSION in place.
/// Add a `case N:` step for each format change (N = the version being upgraded from).
void migrate(json& /*j*/, int fromVersion) {
    for (int v = std::max(fromVersion, 0); v < Config::CURRENT_VERSION; ++v) {
        switch (v) {
        case 0: // Files without a version field: same layout as version 1.
        default:
            break;
        }
    }
}

} // namespace

void Config::sanitize() {
    if (!std::isfinite(maxHudOverlayFlashAlpha)) maxHudOverlayFlashAlpha = Config{}.maxHudOverlayFlashAlpha;
    maxHudOverlayFlashAlpha = std::clamp(maxHudOverlayFlashAlpha, 0.f, 1.f);
}

bool Config::fromJsonText(std::string_view text) {
    *this  = Config{};
    auto j = json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return false;

    int fileVersion = 0;
    read(j, "version", fileVersion);
    migrate(j, fileVersion);

    for (auto const& f : BOOL_FIELDS) read(j, f.key, this->*f.member);
    read(j, "maxHudOverlayFlashAlpha", maxHudOverlayFlashAlpha);
    sanitize();

    json extra = json::object();
    for (auto it = j.begin(); it != j.end(); ++it) {
        if (!isKnownKey(it.key())) extra[it.key()] = it.value();
    }
    if (!extra.empty()) extraFields = extra.dump(-1, ' ', false, json::error_handler_t::replace);
    version = std::max(fileVersion, CURRENT_VERSION);
    return true;
}

std::string Config::toJsonText() const {
    Config c = *this;
    c.sanitize();
    json j       = json::object();
    j["version"] = std::max(c.version, CURRENT_VERSION);
    for (auto const& f : BOOL_FIELDS) j[f.key] = c.*f.member;
    j["maxHudOverlayFlashAlpha"] = c.maxHudOverlayFlashAlpha;
    if (!extraFields.empty()) {
        auto extra = json::parse(extraFields, nullptr, false);
        if (extra.is_object()) {
            for (auto it = extra.begin(); it != extra.end(); ++it) {
                if (!j.contains(it.key())) j[it.key()] = it.value();
            }
        }
    }
    return j.dump(2, ' ', false, json::error_handler_t::replace);
}

bool Config::load(std::filesystem::path const& file) {
    *this     = Config{};
    auto text = readFile(file);
    if (!text) return false;
    return fromJsonText(*text);
}

bool Config::save(std::filesystem::path const& file) const { return writeFileAtomic(file, toJsonText()); }

} // namespace beetroot::core

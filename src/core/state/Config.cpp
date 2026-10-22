#include "core/state/Config.h"

#include "core/state/AtomicWrite.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
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
    std::string ignored;
    return fromJsonText(text, ignored);
}

bool Config::fromJsonText(std::string_view text, std::string& error) {
    *this = Config{};
    error.clear();
    json j;
    try {
        j = json::parse(text);
    } catch (json::exception const& e) {
        error = e.what();
        return false;
    }
    if (!j.is_object()) {
        error = "not a JSON object";
        return false;
    }

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

std::vector<std::string> changedFields(Config const& before, Config const& after) {
    std::vector<std::string> out;
    for (auto const& f : BOOL_FIELDS) {
        if (before.*f.member != after.*f.member) {
            out.push_back(std::string{f.key} + (after.*f.member ? "=true" : "=false"));
        }
    }
    if (before.maxHudOverlayFlashAlpha != after.maxHudOverlayFlashAlpha) {
        char buf[32];
        auto res = std::to_chars(buf, buf + sizeof(buf), after.maxHudOverlayFlashAlpha);
        out.push_back("maxHudOverlayFlashAlpha=" + std::string(buf, res.ptr));
    }
    if (before.version != after.version) out.push_back("version=" + std::to_string(after.version));
    if (before.extraFields != after.extraFields) out.emplace_back("extraFields");
    return out;
}

Config mergeConfig(Config const& base, Config const& ours, Config const& theirs) {
    Config out = theirs;
    for (auto const& f : BOOL_FIELDS) {
        if (ours.*f.member != base.*f.member) out.*f.member = ours.*f.member;
    }
    if (ours.maxHudOverlayFlashAlpha != base.maxHudOverlayFlashAlpha) {
        out.maxHudOverlayFlashAlpha = ours.maxHudOverlayFlashAlpha;
    }
    if (ours.extraFields != base.extraFields) out.extraFields = ours.extraFields;
    out.version = std::max(ours.version, theirs.version);
    out.sanitize();
    return out;
}

bool Config::save(std::filesystem::path const& file) const { return writeFileAtomic(file, toJsonText()); }

} // namespace beetroot::core

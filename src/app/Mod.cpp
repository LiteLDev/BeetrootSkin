#include "app/Mod.h"

#include "app/Module.h"
#include "core/state/AtomicWrite.h"
#include "core/state/FileWatch.h"
#include "mod/Log.h"

#include <exception>
#include <string>
#include <system_error>
#include <vector>

namespace beetroot::app {

namespace {
std::filesystem::path gModDir;
std::filesystem::path gConfigDir;
core::Config          gConfig;
bool                  gDebugDraw = false;

/// Watches config.json for external edits (client thread after load). Its known() stamp is the
/// file state we last read or wrote.
core::FileChangeDetector gWatch;
/// The settings the file held at that point (base of the merge in saveConfig()).
core::Config gSynced;
/// The file at the known stamp failed to parse.
bool gFileInvalid = false;
/// A read of a changed file failed; warn once until a read succeeds.
bool gReadFailed = false;

using Clock = core::FileChangeDetector::Clock;

std::filesystem::path configFile() { return gConfigDir / "config.json"; }

std::string join(std::vector<std::string> const& parts) {
    std::string out;
    for (auto const& p : parts) {
        if (!out.empty()) out += ", ";
        out += p;
    }
    return out;
}

/// Read and parse the config file. nullopt if it can't be read (then `error` is empty) or
/// parsed (then `error` says why).
std::optional<core::Config> readConfigFile(std::string& error) {
    error.clear();
    auto text = core::readFile(configFile());
    if (!text) return std::nullopt;
    core::Config c;
    if (!c.fromJsonText(*text, error)) return std::nullopt;
    return c;
}

/// Before our save replaces a file that changed since we last read it (an edit the poll has
/// not applied yet): merge that edit in, or, if it can't be read, keep it as config.json.bak.
void mergeUnreadEdit(core::FileStamp const& stamp) {
    auto file = configFile();
    if (!stamp.exists || (stamp == gWatch.known() && !gFileInvalid)) return;
    if (stamp != gWatch.known()) {
        std::string error;
        if (auto theirs = readConfigFile(error)) {
            auto merged  = core::mergeConfig(gSynced, gConfig, *theirs);
            auto changes = core::changedFields(gConfig, merged);
            gConfig      = std::move(merged);
            if (!changes.empty()) log().info("Config reloaded before saving: {}", join(changes));
            return;
        }
    }
    auto backup  = file;
    backup      += ".bak";
    std::error_code ec;
    std::filesystem::copy_file(file, backup, std::filesystem::copy_options::overwrite_existing, ec);
    log().warn(
        "Config {} is invalid or unreadable; overwriting it, previous content {}",
        file.string(),
        ec ? "lost (backup failed: " + ec.message() + ")" : "kept in " + backup.string()
    );
}
} // namespace

std::filesystem::path const& modDir() { return gModDir; }
std::filesystem::path const& configDir() { return gConfigDir; }
std::filesystem::path        modFile(std::filesystem::path const& relative) { return gModDir / relative; }

core::Config& config() { return gConfig; }

bool saveConfig() {
    auto file = configFile();
    mergeUnreadEdit(core::statFile(file));
    if (!gConfig.save(file)) {
        // The file is unchanged: leave the watcher alone so a pending edit still gets applied.
        log().warn("Failed to save {}", file.string());
        return false;
    }
    gSynced      = gConfig;
    gFileInvalid = false;
    gWatch.acknowledge(core::statFile(file), Clock::now()); // our own write: never reloaded
    return true;
}

bool pollConfigFile() {
    auto now = Clock::now();
    if (!gWatch.due(now)) return false;
    auto file      = configFile();
    auto stamp     = core::statFile(file);
    auto prevKnown = gWatch.known();
    if (!gWatch.update(stamp, now)) return false;

    if (!stamp.exists) {
        log().info("Config {} was removed; keeping the current settings", file.string());
        gFileInvalid = false;
        return false;
    }
    std::string                 error;
    std::optional<core::Config> next;
    try {
        next = readConfigFile(error);
    } catch (std::exception const& e) {
        error = e.what();
    }
    if (!next && error.empty()) {
        if (!gReadFailed) log().warn("Config {} changed but could not be read; retrying", file.string());
        gReadFailed = true;
        gWatch.acknowledge(prevKnown, now); // still unread: the next poll sees the change again
        return false;
    }
    gReadFailed = false;
    if (!next) {
        // Reported once per change (the detector reports each stamp once). The file is left
        // alone for the user to fix; the next valid save of it is picked up.
        log().warn("Config {} is invalid, keeping the current settings: {}", file.string(), error);
        gFileInvalid = true;
        return false;
    }
    gFileInvalid = false;
    gSynced      = *next;
    auto changes = core::changedFields(gConfig, *next);
    if (changes.empty()) {
        log().info("Config {} changed on disk: no setting changed", file.string());
        return false;
    }
    // Client thread, like every reader of config(): the change applies as one assignment.
    gConfig = std::move(*next);
    log().info("Config reloaded: {}", join(changes));
    return true;
}

bool debugDraw() { return gDebugDraw; }

namespace detail {

void initPaths(std::filesystem::path const& modDir, std::filesystem::path const& configDir) {
    gModDir    = std::filesystem::absolute(modDir);
    gConfigDir = std::filesystem::absolute(configDir);
    std::error_code ec;
    gDebugDraw = std::filesystem::exists(gModDir / "debug_draw", ec);
}

namespace {
void loadConfigFile() {
    auto        stamp = core::statFile(configFile());
    std::string error;
    auto        loaded = readConfigFile(error);
    if (!loaded && stamp.exists) {
        // Keep the file for the user to fix and run with defaults. An unparsable file is
        // picked up live once it is fixed; an unreadable one (locked) is retried by the poll,
        // since its stamp stays unacknowledged.
        if (!error.empty()) {
            log().warn("Config {} is invalid; using defaults: {}", configFile().string(), error);
            gFileInvalid = true;
            gWatch.acknowledge(stamp, Clock::now());
        } else {
            log().warn("Config {} could not be read; using defaults until it can", configFile().string());
        }
        return;
    }
    if (loaded) gConfig = *loaded;
    gSynced = gConfig;
    gWatch.acknowledge(stamp, Clock::now());
    // Write back so new fields appear in the file with their defaults.
    saveConfig();
}
} // namespace

void loadConfig() {
    loadConfigFile();
    // BeetrootSkin::load() (src/mod, which features don't edit) calls this; it is the load-time
    // hook point for Module::onLoad() (e.g. KeyRegistry keys must exist before enable()).
    loadModules();
}

} // namespace detail

} // namespace beetroot::app

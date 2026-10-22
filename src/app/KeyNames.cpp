// Names of BeetrootSkin's KeyRegistry keys in Settings > Keyboard & Mouse.
//
// The controls menu localizes a mod key's action "key.BeetrootSkin.<name>" (and asks for
// "<action>.name" / "<action>.description"); the game's language files don't know them, so this
// module answers those lookups from BeetrootSkin's own lang files: "beetroot.key.<name>" and
// "beetroot.key.<name>.description". Shared by every feature that registers a key.
#include "app/I18n.h"
#include "app/Module.h"
#include "mod/Log.h"

#include "ll/api/memory/Hook.h"

#include "mc/locale/I18nImpl.h"
#include "mc/locale/Localization.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace beetroot::app::key_names {

constexpr std::string_view ACTION_PREFIX = "key.BeetrootSkin."; ///< "key.<mod name>."

/// Logs each id's text once (and again when it changes): proves the controls menu lists our keys and shows
/// the name it got (per language) without per-frame logging. Any thread. The map is leaked on
/// purpose (no static destruction order issues).
namespace {
void logOnce(std::string const& id, std::string const& text) {
    static std::mutex mutex;
    static auto&      logged = *new std::map<std::string, std::string>{}; // id -> last logged text
    std::lock_guard   lock{mutex};
    if (auto it = logged.find(id); it != logged.end()) {
        if (it->second == text) return; // the common case: no allocation
        it->second = text;              // e.g. the game language changed
    } else {
        logged.emplace(id, text);
    }
    log().info("Key name requested: {} -> '{}'", id, text);
}
} // namespace

LL_TYPE_INSTANCE_HOOK(
    KeyNameI18nHook,
    ll::memory::HookPriority::Normal,
    I18nImpl,
    &I18nImpl::$get,
    std::string,
    std::string const&                          id,
    std::vector<std::string> const&             params,
    std::shared_ptr<::Localization const> const locale
) {
    if (id.size() > ACTION_PREFIX.size() && id.starts_with(ACTION_PREFIX)) {
        constexpr std::string_view NAME_SUFFIX = ".name";
        constexpr std::string_view DESC_SUFFIX = ".description";
        auto                       rest        = std::string_view{id}.substr(ACTION_PREFIX.size());
        std::string                text;
        if (rest.ends_with(DESC_SUFFIX)) {
            text = tr("beetroot.key." + std::string{rest}, " ");
        } else {
            if (rest.ends_with(NAME_SUFFIX)) rest.remove_suffix(NAME_SUFFIX.size());
            text = tr("beetroot.key." + std::string{rest}, rest);
        }
        logOnce(id, text);
        return text;
    }
    return origin(id, params, locale);
}

namespace {
class KeyNamesModule final : public Module {
public:
    [[nodiscard]] char const* name() const override { return "key-names"; }
    [[nodiscard]] int         order() const override { return -40; }
    void                      install() override { KeyNameI18nHook::hook(); }
    void                      uninstall() override { KeyNameI18nHook::unhook(); }
} gModule;
BS_REGISTER_MODULE(gModule);
} // namespace

} // namespace beetroot::app::key_names

#pragma once

// Self-registering modules. Every unit that needs hooks or listeners defines one Module and
// registers it with BS_REGISTER_MODULE at namespace scope; BeetrootSkin::enable() installs all
// registered modules (ascending order()) and disable() uninstalls them in reverse. Feature
// units therefore never touch src/mod.
//
//     namespace {
//     class MyFeature final : public beetroot::app::Module {
//     public:
//         char const* name() const override { return "my-feature"; }
//         void install() override { MyHook::hook(); }
//         void uninstall() override { MyHook::unhook(); }
//     } gMyFeature;
//     BS_REGISTER_MODULE(gMyFeature);
//     } // namespace

#include <vector>

namespace beetroot::app {

class Module {
public:
    virtual ~Module() = default;

    [[nodiscard]] virtual char const* name() const = 0;
    /// Install order: lower first. Infrastructure (hooks other modules rely on) uses negative
    /// values; features keep the default 0.
    [[nodiscard]] virtual int order() const { return 0; }
    /// Called once from BeetrootSkin::enable() on the main thread. Hook / subscribe here.
    virtual void install() = 0;
    /// Called once from BeetrootSkin::disable(). Undo everything install() did.
    virtual void uninstall() = 0;
    /// Called once during BeetrootSkin::load() (main thread, after the config is loaded, before
    /// any install()), for work the game needs before mods are enabled, e.g. creating
    /// ll::input::KeyRegistry keys (the game reads them into its keyboard layout before
    /// enable()). Translations are not loaded yet (app::tr() returns fallbacks). Runs once per
    /// process, even across disable/enable: install() must redo anything uninstall() undoes.
    /// Default: nothing.
    virtual void onLoad() {}
};

/// Run onLoad() of every module (sorted by order()); a module that throws is logged and skipped.
/// Called once from app::detail::loadConfig() (i.e. BeetrootSkin::load()).
void loadModules();

/// All registered modules (registration order). Static-init safe.
[[nodiscard]] std::vector<Module*>& modules();

/// Install every module (sorted by order()); a module that throws is logged and skipped.
void installModules();
/// Uninstall installed modules in reverse install order.
void uninstallModules();

struct ModuleRegistrar {
    explicit ModuleRegistrar(Module& module) { modules().push_back(&module); }
};

} // namespace beetroot::app

#define BS_DETAIL_CONCAT2(a, b) a##b
#define BS_DETAIL_CONCAT(a, b)  BS_DETAIL_CONCAT2(a, b)
/// Register a Module instance (an object with static storage duration).
#define BS_REGISTER_MODULE(obj)                                                                                        \
    [[maybe_unused]] static ::beetroot::app::ModuleRegistrar BS_DETAIL_CONCAT(bsModuleRegistrar_, __LINE__) { obj }

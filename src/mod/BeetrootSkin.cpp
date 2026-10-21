#include "mod/BeetrootSkin.h"

#include "app/I18n.h"
#include "app/Mod.h"
#include "app/Module.h"
#include "mod/Log.h"

#include "ll/api/mod/RegisterHelper.h"

namespace beetroot {

ll::io::Logger& log() { return BeetrootSkin::getInstance().getSelf().getLogger(); }

BeetrootSkin& BeetrootSkin::getInstance() {
    static BeetrootSkin instance;
    return instance;
}

bool BeetrootSkin::load() {
    // Flush every message so the e2e harness (and people tailing logs) see them immediately.
    getSelf().getLogger().setFlushLevel(ll::io::LogLevel::Trace);
    app::detail::initPaths(getSelf().getModDir(), getSelf().getConfigDir());
    app::detail::loadConfig();
    app::loadTranslations(getSelf().getModDir() / "lang");
    if (app::debugDraw()) log().info("Debug drawing enabled (debug_draw marker present)");
    return true;
}

bool BeetrootSkin::enable() {
    app::installModules();
    log().info("BeetrootSkin enabled");
    return true;
}

bool BeetrootSkin::disable() {
    app::uninstallModules(); // modules that change config() call app::saveConfig() themselves
    return true;
}

} // namespace beetroot

LL_REGISTER_MOD(beetroot::BeetrootSkin, beetroot::BeetrootSkin::getInstance());

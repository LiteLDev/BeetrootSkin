// Live config reload, like AppleSkin's (whose config screen applies changes immediately):
// config.json may be edited at any time while the game runs. The poll runs on the client thread
// after every UI render (main menu, pause screens and in-world alike; level ticks stop while the
// game is paused). app::pollConfigFile() only compares time points per call and stats the file
// about once a second.
#include "app/Mod.h"
#include "app/Module.h"
#include "mod/Log.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/render/UIRenderEvent.h"

#include <exception>

namespace beetroot::app {

namespace {

class ConfigWatchModule final : public Module {
public:
    [[nodiscard]] char const* name() const override { return "config-watch"; }
    [[nodiscard]] int         order() const override { return -50; }

    void install() override {
        mRender = ll::event::EventBus::getInstance().emplaceListener<ll::event::AfterUIRenderEvent>(
            [](ll::event::AfterUIRenderEvent&) {
                try {
                    pollConfigFile();
                } catch (std::exception const& e) {
                    static bool logged = false;
                    if (!logged) log().error("Config reload failed: {}", e.what());
                    logged = true;
                }
            }
        );
    }

    void uninstall() override {
        if (mRender) ll::event::EventBus::getInstance().removeListener(mRender);
        mRender.reset();
    }

private:
    ll::event::ListenerPtr mRender;
} gModule;
BS_REGISTER_MODULE(gModule);

} // namespace

} // namespace beetroot::app

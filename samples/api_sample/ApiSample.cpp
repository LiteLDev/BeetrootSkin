// beetroot-api-sample: a tiny, test-only LeviLamina mod that uses BeetrootSkin's public API from
// another DLL (built only with `xmake f --api_sample=y`, never shipped). The e2e scenario
// tools/e2e/scenarios/api.ps1 deploys it next to BeetrootSkin. It
// - doubles the nutrition of minecraft:bread (FoodValuesEvent),
// - cancels the exhaustion underlay (HUDOverlayEvent::Exhaustion),
// - removes the food values from minecraft:apple tooltips (TooltipOverlayEvent::Pre).
#include "beetrootskin/api/Events.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/ListenerBase.h"
#include "ll/api/io/Logger.h"
#include "ll/api/mod/NativeMod.h"
#include "ll/api/mod/RegisterHelper.h"

#include "mc/world/item/ItemStackBase.h"

#include <utility>
#include <vector>

namespace beetroot_api_sample {

namespace api = beetroot::api;

class ApiSample {
public:
    static ApiSample& getInstance() {
        static ApiSample instance;
        return instance;
    }

    ApiSample() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::io::Logger& logger() const { return mSelf.getLogger(); }

    bool load() {
        logger().setFlushLevel(ll::io::LogLevel::Trace);
        return true;
    }

    bool enable() {
        // Calls into BeetrootSkin.dll through its import library.
        logger().info("API sample: BeetrootSkin API version {}", api::apiVersion());
        auto& bus = ll::event::EventBus::getInstance();

        add(bus.emplaceListener<api::FoodValuesEvent>([this](api::FoodValuesEvent& ev) {
            if (ev.itemStack().getTypeName() != "minecraft:bread") return;
            ev.modifiedFoodValues().hunger = ev.defaultFoodValues().hunger * 2;
            if (std::exchange(mLoggedBread, true)) return;
            logger().info(
                "API sample: doubled minecraft:bread nutrition {} -> {}",
                ev.defaultFoodValues().hunger,
                ev.modifiedFoodValues().hunger
            );
        }));
        add(bus.emplaceListener<api::HUDOverlayEvent::Exhaustion>([this](api::HUDOverlayEvent::Exhaustion& ev) {
            ev.cancel();
            if (std::exchange(mLoggedExhaustion, true)) return;
            logger().info(
                "API sample: cancelled HUDOverlayEvent::Exhaustion (exhaustion={:.2f} x={} y={})",
                ev.exhaustion(),
                ev.x(),
                ev.y()
            );
        }));
        add(bus.emplaceListener<api::TooltipOverlayEvent::Pre>([this](api::TooltipOverlayEvent::Pre& ev) {
            if (ev.itemStack().getTypeName() != "minecraft:apple") return;
            ev.cancel();
            if (std::exchange(mLoggedApple, true)) return;
            logger().info(
                "API sample: cancelled TooltipOverlayEvent::Pre for minecraft:apple (hunger={})",
                ev.modifiedFood().hunger
            );
        }));

        logger().info("API sample: {} of 3 listeners subscribed", mListeners.size());
        return true;
    }

    bool disable() {
        auto& bus = ll::event::EventBus::getInstance();
        for (auto& l : mListeners) bus.removeListener(l);
        mListeners.clear();
        return true;
    }

private:
    void add(ll::event::ListenerPtr l) {
        if (l) mListeners.push_back(std::move(l));
        else logger().error("API sample: subscribing failed (is BeetrootSkin loaded?)");
    }

    ll::mod::NativeMod&                 mSelf;
    std::vector<ll::event::ListenerPtr> mListeners;
    bool                                mLoggedBread = false, mLoggedExhaustion = false, mLoggedApple = false;
};

} // namespace beetroot_api_sample

LL_REGISTER_MOD(beetroot_api_sample::ApiSample, beetroot_api_sample::ApiSample::getInstance());

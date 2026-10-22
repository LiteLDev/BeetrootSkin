// BeetrootSkin's public API (include/beetrootskin/api): registers the AppleSkin-style events
// with LeviLamina's EventBus and implements the exported functions.
//
// LeviLamina only accepts listeners for events that have an emitter registered
// (EventBus::setEventEmitter). BeetrootSkin posts these events itself, so the emitter is an empty
// EmitterBase; it is registered in onLoad() (BeetrootSkin::load()), before dependent mods are
// enabled and subscribe. Event identity across DLLs comes from each class's CustomEventId string.
#include "api/Post.h"

#include "beetrootskin/api/Events.h"

#include "app/Module.h"
#include "core/food/ApiFoodValues.h"
#include "game/FoodValues.h"
#include "mod/Log.h"

#include "ll/api/event/EmitterBase.h"
#include "ll/api/event/EventBus.h"

#include <map>
#include <memory>
#include <string>

namespace beetroot::api_impl {

namespace {

template <class E>
void registerEvent() {
    bool ok = ll::event::EventBus::getInstance().setEventEmitter<E>([] {
        return std::make_unique<ll::event::EmitterBase>();
    });
    if (!ok) log().warn("API: could not register {}", E::CustomEventId.name);
}

class ApiModule final : public app::Module {
public:
    [[nodiscard]] char const* name() const override { return "api"; }
    [[nodiscard]] int         order() const override { return -200; }

    void onLoad() override {
        registerEvent<api::FoodValuesEvent>();
        registerEvent<api::TooltipOverlayEvent::Pre>();
        registerEvent<api::TooltipOverlayEvent::Render>();
        registerEvent<api::HUDOverlayEvent::Exhaustion>();
        registerEvent<api::HUDOverlayEvent::Saturation>();
        registerEvent<api::HUDOverlayEvent::HungerRestored>();
        registerEvent<api::HUDOverlayEvent::HealthRestored>();
        log().info("API: events registered (version {})", api::API_VERSION);
    }
    void install() override {}
    void uninstall() override {}
} gModule;
BS_REGISTER_MODULE(gModule);

} // namespace

void traceCancelled(std::string_view name, bool cancelled) {
    static std::map<std::string, bool, std::less<>> last;
    auto                                            it = last.find(name);
    if (it == last.end()) {
        if (!cancelled) return; // first post, not cancelled: nothing to report
        it = last.emplace(std::string{name}, false).first;
    }
    if (it->second == cancelled) return;
    it->second = cancelled;
    log().info("API: {} {}", name, cancelled ? "cancelled by a listener" : "no longer cancelled");
}

} // namespace beetroot::api_impl

namespace beetroot::api {

int apiVersion() noexcept { return API_VERSION; }

std::optional<QueriedFood> queryFoodValues(ItemStackBase const& stack, Player* player) {
    auto q = game::queryFoodValues(stack, player);
    if (!q) return std::nullopt;
    return QueriedFood{
        core::toApiFoodValues(q->defaultValues),
        core::toApiFoodValues(q->modifiedValues),
        q->modifiedValues.rotten
    };
}

} // namespace beetroot::api

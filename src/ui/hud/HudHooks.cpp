// HUD plumbing: hooks the vanilla hunger and heart renderers, computes icon rectangles and runs
// the registered IHudLayers around them. Also drives the AppleSkin flash animation per tick.
#include "ui/hud/HudHooks.h"

#include "app/Mod.h"
#include "app/Module.h"
#include "core/geom/IconRows.h"
#include "mod/Log.h"
#include "ui/gfx/Painter.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/ClientLevelTickEvent.h"
#include "ll/api/memory/Hook.h"

#include "mc/client/gui/controls/UIControl.h"
#include "mc/client/gui/controls/renderers/HudHeartRenderer.h"
#include "mc/client/gui/controls/renderers/HudHungerRenderer.h"
#include "mc/client/gui/screens/UIScene.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <string>

namespace beetroot::ui {

namespace hud_detail {

constexpr float ICON = 9;

core::FlashAnimator gFlash;
int                 gTick = 0;

/// Last computed icon layouts (GUI units), shared between the two renderers so a hearts layer
/// can also know where the hunger bar is and vice versa.
std::array<core::Rect, core::ICONS_PER_ROW> gHungerIcons{};
std::array<float, core::ICONS_PER_ROW>      gHungerJitter{};
bool                                        gHungerValid = false;
std::vector<core::Rect>                     gHeartIcons;
std::vector<float>                          gHeartJitter;

bool isHudScene(MinecraftUIRenderContext& ctx) {
    auto name = ctx.mCurrentScene.getScreenName();
    return name.starts_with("hud_screen");
}

core::Rect controlRect(UIControl& owner) {
    owner._setCachedPosition();
    glm::vec2 pos  = owner.mCachedPosition;
    glm::vec2 size = owner.mSize;
    return {pos.x, pos.y, size.x, size.y};
}

void logOnce(char const* what, std::exception const* e) {
    static std::string last;
    std::string        msg = std::string{what} + (e ? e->what() : "unknown error");
    if (msg == last) return;
    last = msg;
    log().error("HUD layer failed: {}", msg);
}

void runLayers(HudSlot slot, MinecraftUIRenderContext& ctx, core::Rect const& control) {
    auto& layers = hudLayers();
    bool  any    = std::any_of(layers.begin(), layers.end(), [&](IHudLayer* l) { return l->slot() == slot; });
    if (!any) return;

    std::vector<IHudLayer*> matching;
    for (auto* l : layers) {
        if (l->slot() == slot) matching.push_back(l);
    }
    std::stable_sort(matching.begin(), matching.end(), [](auto* a, auto* b) { return a->order() < b->order(); });

    Painter    painter(ctx);
    HudContext hc{painter, slot, control};
    hc.hungerIcons      = gHungerIcons;
    hc.hungerIconsValid = gHungerValid;
    hc.hungerJitterY    = gHungerJitter;
    hc.heartIcons       = gHeartIcons;
    hc.heartJitterY     = gHeartJitter;
    hc.state            = game::readFoodState();
    hc.held             = game::heldFood();
    hc.flashAlpha       = hudFlashAlpha();
    hc.clientTick       = gTick;
    for (auto* l : matching) {
        try {
            l->render(hc);
            painter.flush();
        } catch (std::exception const& e) {
            logOnce("", &e);
        } catch (...) {
            logOnce("", nullptr);
        }
    }
}

/// Resting lines of the two bars, learned from still frames (see core::RestingRows). Vanilla
/// lays the first row out at the owner control's origin (calibration logs: icon y 0 at rest for
/// both bars), which is the fallback until a still frame has been seen.
core::RestingRows gHungerRest;
core::RestingRows gHeartRest;
constexpr float   ROW0_REST_Y = 0;

/// Logs the range of jitter offsets seen for a bar whenever it widens (never per frame), as
/// evidence of vanilla's shake for the e2e harness. Only offsets measured against a learned
/// resting line count (not the fallback).
struct JitterRange {
    float lo = 0, hi = 0;
    template <class Offsets>
    void observe(char const* bar, core::RestingRows const& rest, Offsets const& offsets) {
        if (!rest.learned(0)) return;
        float newLo = lo, newHi = hi;
        for (float o : offsets) newLo = std::min(newLo, o), newHi = std::max(newHi, o);
        if (newLo == lo && newHi == hi) return;
        lo = newLo, hi = newHi;
        log().info("{} jitter seen: offsets {}..{}", bar, lo, hi);
    }
};
JitterRange gHungerRange, gHeartRange;

/// Hunger icons from the renderer's per-icon positions (relative to the owner control).
/// Vanilla fills mIconPosition[i] for shank i counted from the right.
void updateHungerLayout(HudHungerRenderer const& r, core::Rect const& control) {
    auto const&       pos = r.mIconPosition.get();
    static core::Rect restFor{};
    if (control != restFor) gHungerRest.reset(), restFor = control;
    static std::vector<float> ys(pos.size());
    for (std::size_t i = 0; i < pos.size(); ++i) ys[i] = pos[i].y;
    auto const& rest = gHungerRest.update(ys, core::ICONS_PER_ROW, ROW0_REST_Y);
    for (std::size_t i = 0; i < pos.size(); ++i) {
        gHungerIcons[i]  = {control.x + pos[i].x, control.y + pos[i].y, ICON, ICON};
        gHungerJitter[i] = ys[i] - rest[i];
    }
    gHungerRange.observe("Hunger", gHungerRest, gHungerJitter);
    gHungerValid = true;
    // Calibration log: on layout changes (without jitter) only.
    static core::Rect lastControl{};
    static float      lastX0 = -1;
    if (control != lastControl || pos[0].x != lastX0) {
        lastControl = control;
        lastX0      = pos[0].x;
        log().info(
            "Hunger bar: control {},{} {}x{}; icon[0] {},{} icon[9] {},{}",
            control.x,
            control.y,
            control.w,
            control.h,
            pos[0].x,
            pos[0].y,
            pos[9].x,
            pos[9].y
        );
    }
}

void updateHeartLayout(HudHeartRenderer const& r, core::Rect const& control) {
    auto const&       pos = r.mIconPosition.get();
    static core::Rect restFor{};
    if (control != restFor) gHeartRest.reset(), restFor = control;
    static std::vector<float> ys;
    ys.resize(pos.size());
    for (std::size_t i = 0; i < pos.size(); ++i) ys[i] = pos[i].y;
    auto const& rest = gHeartRest.update(ys, core::ICONS_PER_ROW, ROW0_REST_Y);
    gHeartIcons.resize(pos.size());
    gHeartJitter.resize(pos.size());
    for (std::size_t i = 0; i < pos.size(); ++i) {
        gHeartIcons[i]  = {control.x + pos[i].x, control.y + pos[i].y, ICON, ICON};
        gHeartJitter[i] = ys[i] - rest[i];
    }
    gHeartRange.observe("Heart", gHeartRest, gHeartJitter);
    static std::size_t lastCount = static_cast<std::size_t>(-1);
    static core::Rect  lastControl{};
    if (pos.size() != lastCount || control != lastControl) {
        lastCount   = pos.size();
        lastControl = control;
        log().info(
            "Hearts: control {},{} {}x{}; {} icons; icon[0] {},{}",
            control.x,
            control.y,
            control.w,
            control.h,
            pos.size(),
            pos.empty() ? 0.f : pos[0].x,
            pos.empty() ? 0.f : pos[0].y
        );
    }
}

LL_TYPE_INSTANCE_HOOK(
    HungerRenderHook,
    ll::memory::HookPriority::Normal,
    HudHungerRenderer,
    &HudHungerRenderer::$render,
    void,
    ::MinecraftUIRenderContext& ctx,
    ::IClientInstance&          client,
    ::UIControl&                owner,
    int                         pass
) {
    if (!isHudScene(ctx)) return origin(ctx, client, owner, pass);
    auto control = controlRect(owner);
    updateHungerLayout(*this, control);
    runLayers(HudSlot::BelowHunger, ctx, control);
    origin(ctx, client, owner, pass);
    runLayers(HudSlot::AboveHunger, ctx, control);
}

LL_TYPE_INSTANCE_HOOK(
    HeartRenderHook,
    ll::memory::HookPriority::Normal,
    HudHeartRenderer,
    &HudHeartRenderer::$render,
    void,
    ::MinecraftUIRenderContext& ctx,
    ::IClientInstance&          client,
    ::UIControl&                owner,
    int                         pass
) {
    if (!isHudScene(ctx)) return origin(ctx, client, owner, pass);
    auto control = controlRect(owner);
    updateHeartLayout(*this, control);
    runLayers(HudSlot::BelowHearts, ctx, control);
    origin(ctx, client, owner, pass);
    runLayers(HudSlot::AboveHearts, ctx, control);
}

class HudModule final : public app::Module {
public:
    [[nodiscard]] char const* name() const override { return "hud-hooks"; }
    [[nodiscard]] int         order() const override { return -50; }

    void install() override {
        mTick = ll::event::EventBus::getInstance().emplaceListener<ll::event::ClientLevelTickEvent>(
            [](ll::event::ClientLevelTickEvent&) {
                ++gTick;
                try {
                    gFlash.tick(game::heldFood().has_value());
                } catch (...) {
                    gFlash.reset();
                }
            }
        );
        HungerRenderHook::hook();
        HeartRenderHook::hook();
    }

    void uninstall() override {
        HeartRenderHook::unhook();
        HungerRenderHook::unhook();
        if (mTick) ll::event::EventBus::getInstance().removeListener(mTick);
        mTick.reset();
    }

private:
    ll::event::ListenerPtr mTick;
} gModule;
BS_REGISTER_MODULE(gModule);

} // namespace hud_detail

std::vector<IHudLayer*>& hudLayers() {
    static std::vector<IHudLayer*> instance;
    return instance;
}

int clientTickCount() { return hud_detail::gTick; }

float hudFlashAlpha() { return hud_detail::gFlash.alpha(app::config().maxHudOverlayFlashAlpha); }

} // namespace beetroot::ui

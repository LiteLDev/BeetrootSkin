// AppleSkin's saturation HUD overlay (HUDOverlayHandler.drawSaturationOverlay):
// - base overlay: the current saturation as golden outlines over the hunger shanks (alpha 1);
// - gain preview: the saturation the held food would add, flashing at the food-values alpha,
//   starting at the current saturation's last icon.
#include "core/food/SaturationOverlay.h"
#include "api/Post.h"
#include "app/Mod.h"
#include "mod/Log.h"
#include "ui/gfx/Painter.h"
#include "ui/hud/HudHooks.h"

#include "beetrootskin/api/event/HUDOverlayEvent.h"

#include <cmath>
#include <vector>

namespace beetroot::features::saturation {

namespace {

constexpr float ICON = static_cast<float>(core::sprites::HUD_ICON_SIZE);

/// Draw `icons` over the hunger bar at `alpha`. With showVanillaAnimationsOverlay off the vanilla
/// jitter is removed so the outlines stay still (AppleSkin only follows the shake when enabled).
/// `dx`/`dy` move the icons (a HUDOverlayEvent::Saturation listener changed x/y).
void drawIcons(ui::HudContext& ctx, std::vector<core::SaturationIcon> const& icons, float alpha, float dx, float dy) {
    if (icons.empty() || !(alpha > 0)) return;
    bool followJitter = app::config().showVanillaAnimationsOverlay;
    for (auto const& icon : icons) {
        if (icon.index < 0 || icon.index >= core::ICONS_PER_ROW) continue;
        auto       i   = static_cast<std::size_t>(icon.index);
        core::Rect dst = ctx.hungerIcons[i];
        if (!followJitter) dst.y -= ctx.hungerJitterY[i];
        dst.x += dx;
        dst.y += dy;
        ctx.painter.texture(
            ui::textures::ICONS,
            dst,
            {static_cast<float>(icon.u), static_cast<float>(core::sprites::HUD_SATURATION_V), ICON, ICON},
            core::sprites::ICONS_TEXTURE_SIZE,
            core::sprites::ICONS_TEXTURE_SIZE,
            ui::palette::WHITE.withAlpha(alpha)
        );
    }
}

/// The hunger bar is drawable and the player has a hunger bar (not creative/spectator).
bool hungerBarVisible(ui::HudContext const& ctx) {
    return ctx.hungerIconsValid && ctx.state && !ctx.state->creativeLike;
}

/// AppleSkin posts one HUDOverlayEvent.Saturation for the whole saturation overlay (current
/// saturation and the held food's gain). The base layer (order 0) posts it every hunger draw and
/// leaves the outcome here for the gain layer (order 20) of the same draw.
struct SaturationEventResult {
    bool  allowed = false;
    float dx = 0, dy = 0;
};
SaturationEventResult gSaturationEvent;

SaturationEventResult postSaturationEvent(ui::HudContext& ctx) {
    float                            x0 = ctx.hungerIcons[0].x + ctx.hungerIcons[0].w; // right edge of the hunger bar
    float                            y0 = ctx.hungerIcons[0].y - ctx.hungerJitterY[0]; // its resting top
    api::HUDOverlayEvent::Saturation ev{ctx.state->saturation, x0, y0, ctx.painter.raw()};
    if (!api_impl::postCancellable(ev)) return {};
    return {true, ev.x() - x0, ev.y() - y0};
}

/// What a layer drew last (inputs in 0.01 steps), so changes are logged once, never per frame.
struct Drawn {
    long        saturation                             = -1;
    long        gain                                   = -1;
    std::size_t count                                  = 0;
    friend bool operator==(Drawn const&, Drawn const&) = default;
};

/// Logs the drawn icons when they change, for e2e assertions (count 0 = nothing drawn).
void logChange(char const* what, Drawn& last, float saturation, float gain, std::size_t count) {
    Drawn now = count == 0 ? Drawn{} : Drawn{std::lround(saturation * 100), std::lround(gain * 100), count};
    if (now == last) return;
    last = now;
    if (count == 0) {
        log().info("Saturation {}: none", what);
        return;
    }
    log().info("Saturation {}: saturation={:.2f} gain={:.2f} icons={}", what, saturation, gain, count);
}

} // namespace

/// Base overlay: the current saturation at full opacity.
class SaturationBaseLayer final : public ui::IHudLayer {
public:
    [[nodiscard]] ui::HudSlot slot() const override { return ui::HudSlot::AboveHunger; }
    [[nodiscard]] int         order() const override { return 0; }

    void render(ui::HudContext& ctx) override {
        gSaturationEvent = {};
        if (!app::config().showSaturationHudOverlay || !hungerBarVisible(ctx)) {
            logChange("overlay", mLast, 0, 0, 0);
            return;
        }
        gSaturationEvent = postSaturationEvent(ctx);
        if (!gSaturationEvent.allowed) {
            logChange("overlay", mLast, 0, 0, 0);
            return;
        }
        float sat   = ctx.state->saturation;
        auto  icons = core::saturationOverlaySprites(sat, 0);
        logChange("overlay", mLast, sat, 0, icons.size());
        drawIcons(ctx, icons, 1.f, gSaturationEvent.dx, gSaturationEvent.dy);
    }

private:
    Drawn mLast;
};

/// Gain preview: the saturation the held food would add, at the flashing food-values alpha.
/// AppleSkin draws it only when both the food-values overlay and the saturation overlay are on.
class SaturationGainLayer final : public ui::IHudLayer {
public:
    [[nodiscard]] ui::HudSlot slot() const override { return ui::HudSlot::AboveHunger; }
    [[nodiscard]] int         order() const override { return 20; }

    void render(ui::HudContext& ctx) override {
        auto const& cfg = app::config();
        // gSaturationEvent.allowed: the base layer of this draw posted HUDOverlayEvent::Saturation
        // (so the overlay is enabled and visible) and no listener cancelled it.
        if (!cfg.showFoodValuesHudOverlay || !cfg.showSaturationHudOverlay || !gSaturationEvent.allowed
            || !hungerBarVisible(ctx) || !ctx.held) {
            logChange("preview", mLast, 0, 0, 0);
            return;
        }
        float                             sat  = ctx.state->saturation;
        float                             gain = core::saturationPreviewGain(*ctx.state, ctx.held->values);
        std::vector<core::SaturationIcon> icons;
        if (gain > 0) icons = core::saturationOverlaySprites(sat, gain);
        logChange("preview", mLast, sat, gain, icons.size());
        drawIcons(ctx, icons, ctx.flashAlpha, gSaturationEvent.dx, gSaturationEvent.dy);
    }

private:
    Drawn mLast;
};

namespace {
SaturationBaseLayer gBaseLayer;
SaturationGainLayer gGainLayer;
} // namespace
BS_REGISTER_HUD_LAYER(gBaseLayer);
BS_REGISTER_HUD_LAYER(gGainLayer);

} // namespace beetroot::features::saturation

// AppleSkin's exhaustion underlay: a strip from icons.png drawn under the hunger bar, growing
// from the right edge of the rightmost shank to the left as exhaustion rises from 0 to 4.
// (AppleSkin HUDOverlayHandler.drawExhaustionOverlay: u = 81 - width, v = 18, alpha 0.75.)
#include "api/Post.h"
#include "app/Mod.h"
#include "core/food/FoodMath.h"
#include "mod/Log.h"
#include "ui/gfx/Painter.h"
#include "ui/hud/HudHooks.h"

#include "beetrootskin/api/event/HUDOverlayEvent.h"

namespace beetroot::features::exhaustion {

namespace {

constexpr float ALPHA = 0.75f;

class ExhaustionUnderlay final : public ui::IHudLayer {
public:
    [[nodiscard]] ui::HudSlot slot() const override { return ui::HudSlot::BelowHunger; }

    void render(ui::HudContext& ctx) override {
        bool  visible = shouldDraw(ctx);
        float right = 0, top = 0; // event position (AppleSkin: x = right edge of the bar, y = top)
        if (visible) {
            auto const& icon0 = ctx.hungerIcons[0];
            api::HUDOverlayEvent::Exhaustion
                ev{ctx.state->exhaustion, icon0.x + icon0.w, restingY(ctx), ctx.painter.raw()};
            if (!api_impl::postCancellable(ev)) {
                logChanges(ctx, false, 0, true);
                return;
            }
            right = ev.x();
            top   = ev.y();
        }
        int width = visible ? core::sprites::exhaustionBarWidth(ctx.state->exhaustion) : 0;
        logChanges(ctx, visible, width, false);
        if (!visible || width <= 0) return;

        float x = right - static_cast<float>(width);
        float y = top;
        ctx.painter.texture(
            ui::textures::ICONS,
            {x, y, static_cast<float>(width), static_cast<float>(core::sprites::HUD_ICON_SIZE)},
            {static_cast<float>(core::sprites::EXHAUSTION_BAR_WIDTH - width),
             static_cast<float>(core::sprites::EXHAUSTION_BAR_V),
             static_cast<float>(width),
             static_cast<float>(core::sprites::HUD_ICON_SIZE)},
            core::sprites::ICONS_TEXTURE_SIZE,
            core::sprites::ICONS_TEXTURE_SIZE,
            ui::palette::WHITE.withAlpha(ALPHA)
        );
    }

private:
    // Resting y of the bar: HudHooks reports each shank's vanilla jitter relative to the learned
    // resting line, so removing it from the rightmost icon gives the bar's resting y.
    static float restingY(ui::HudContext const& ctx) { return ctx.hungerIcons[0].y - ctx.hungerJitterY[0]; }

    static bool shouldDraw(ui::HudContext const& ctx) {
        if (!app::config().showFoodExhaustionHudUnderlay) return false;
        if (!ctx.hungerIconsValid || !ctx.hungerBarShown || !ctx.state) return false;
        // Vanilla shows no hunger bar in creative/spectator; AppleSkin skips its overlays there.
        if (ctx.state->creativeLike) return false;
        return true;
    }

    // State-change logging for the e2e scenario: visibility, and the fill in ~1/9 steps.
    void logChanges(ui::HudContext const& ctx, bool visible, int width, bool cancelled) {
        int bucket = visible ? width / core::sprites::HUD_ICON_SIZE : (cancelled ? -3 : -1);
        if (bucket == mLastBucket && visible == mLastVisible) return;
        mLastBucket  = bucket;
        mLastVisible = visible;
        if (cancelled) {
            log().info("Exhaustion underlay: hidden (cancelled by HUDOverlayEvent::Exhaustion listener)");
            return;
        }
        if (!visible) {
            log().info(
                "Exhaustion underlay: hidden (enabled={} barShown={} creative={} riding={})",
                app::config().showFoodExhaustionHudUnderlay,
                ctx.hungerBarShown,
                ctx.state && ctx.state->creativeLike,
                ctx.state && ctx.state->riding
            );
            return;
        }
        log().info(
            "Exhaustion underlay: width={} exhaustion={:.3f} source={}",
            width,
            ctx.state->exhaustion,
            ctx.state->exact ? "integrated-server" : "client"
        );
    }

    int  mLastBucket  = -2;
    bool mLastVisible = false;
} gLayer;
BS_REGISTER_HUD_LAYER(gLayer);

} // namespace

} // namespace beetroot::features::exhaustion

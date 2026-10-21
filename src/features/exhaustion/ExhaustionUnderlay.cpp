// AppleSkin's exhaustion underlay: a strip from icons.png drawn under the hunger bar, growing
// from the right edge of the rightmost shank to the left as exhaustion rises from 0 to 4.
// (AppleSkin HUDOverlayHandler.drawExhaustionOverlay: u = 81 - width, v = 18, alpha 0.75.)
#include "app/Mod.h"
#include "core/food/FoodMath.h"
#include "mod/Log.h"
#include "ui/gfx/Painter.h"
#include "ui/hud/HudHooks.h"

#include <algorithm>
#include <cmath>

namespace beetroot::features::exhaustion {

namespace {

constexpr float ALPHA = 0.75f;

class ExhaustionUnderlay final : public ui::IHudLayer {
public:
    [[nodiscard]] ui::HudSlot slot() const override { return ui::HudSlot::BelowHunger; }

    void render(ui::HudContext& ctx) override {
        bool visible = shouldDraw(ctx);
        int  width   = visible ? core::sprites::exhaustionBarWidth(ctx.state->exhaustion) : 0;
        logChanges(ctx, visible, width);
        if (!visible || width <= 0) return;

        auto const& right = ctx.hungerIcons[0];
        float       x     = right.x + right.w - static_cast<float>(width);
        float       y     = restingY(ctx);
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
    // Resting y of the bar: vanilla shakes the shanks individually when saturation is 0, so use
    // the y of the last frame in which all icons were aligned (for the same bar position).
    float restingY(ui::HudContext const& ctx) {
        auto const& icons = ctx.hungerIcons;
        bool        aligned =
            std::all_of(ctx.hungerJitterY.begin(), ctx.hungerJitterY.end(), [](float j) { return j == 0.f; });
        if (aligned) {
            mRestX   = icons[0].x;
            mRestY   = icons[0].y;
            mHasRest = true;
            return mRestY;
        }
        if (mHasRest && mRestX == icons[0].x && std::abs(mRestY - icons[0].y) <= 2.f) return mRestY;
        // No aligned frame seen for this position yet: the most common y is the best guess.
        mHasRest        = false;
        float best      = icons[0].y;
        int   bestCount = 0;
        for (auto const& a : icons) {
            int n =
                static_cast<int>(std::count_if(icons.begin(), icons.end(), [&](auto const& b) { return b.y == a.y; }));
            if (n > bestCount) {
                bestCount = n;
                best      = a.y;
            }
        }
        return best;
    }

    static bool shouldDraw(ui::HudContext const& ctx) {
        if (!app::config().showFoodExhaustionHudUnderlay) return false;
        if (!ctx.hungerIconsValid || !ctx.hungerBarShown || !ctx.state) return false;
        // Vanilla shows no hunger bar in creative/spectator; AppleSkin skips its overlays there.
        if (ctx.state->creativeLike) return false;
        return true;
    }

    // State-change logging for the e2e scenario: visibility, and the fill in ~1/9 steps.
    void logChanges(ui::HudContext const& ctx, bool visible, int width) {
        int bucket = visible ? width / core::sprites::HUD_ICON_SIZE : -1;
        if (bucket == mLastBucket && visible == mLastVisible) return;
        mLastBucket  = bucket;
        mLastVisible = visible;
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

    float mRestX = 0, mRestY = 0;
    bool  mHasRest     = false;
    int   mLastBucket  = -2;
    bool  mLastVisible = false;
} gLayer;
BS_REGISTER_HUD_LAYER(gLayer);

} // namespace

} // namespace beetroot::features::exhaustion

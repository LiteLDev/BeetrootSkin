// AppleSkin HUDOverlayHandler "HungerRestored": while the player holds food they can eat, the
// shanks the food would fill flash on the hunger bar (empty shank at alpha/4, then the full or
// half shank at alpha; rotten food uses the green "hunger effect" sprites).
#include "core/food/HungerPreview.h"

#include "app/Mod.h"
#include "mod/Log.h"
#include "ui/gfx/Painter.h"
#include "ui/hud/HudHooks.h"

#include <string>

namespace beetroot::features::hunger_preview {

namespace {

ui::TextureRef const BACKGROUND        = ui::TextureRef::pack("textures/ui/hunger_background");
ui::TextureRef const FULL              = ui::TextureRef::pack("textures/ui/hunger_full");
ui::TextureRef const HALF              = ui::TextureRef::pack("textures/ui/hunger_half");
ui::TextureRef const EFFECT_BACKGROUND = ui::TextureRef::pack("textures/ui/hunger_effect_background");
ui::TextureRef const EFFECT_FULL       = ui::TextureRef::pack("textures/ui/hunger_effect_full");
ui::TextureRef const EFFECT_HALF       = ui::TextureRef::pack("textures/ui/hunger_effect_half");

ui::TextureRef const& spriteFor(core::ShankSprite s) {
    switch (s) {
    case core::ShankSprite::Half:
        return HALF;
    case core::ShankSprite::EffectFull:
        return EFFECT_FULL;
    case core::ShankSprite::EffectHalf:
        return EFFECT_HALF;
    case core::ShankSprite::Full:
    default:
        return FULL;
    }
}

/// Logs the shown range on changes only (used by the e2e scenario).
void traceState(std::string const& now) {
    static std::string last;
    if (now == last) return;
    last = now;
    log().info("Hunger preview: {}", now);
}

} // namespace

class HungerPreviewLayer final : public ui::IHudLayer {
public:
    [[nodiscard]] ui::HudSlot slot() const override { return ui::HudSlot::AboveHunger; }
    /// Between the saturation base overlay (0) and the saturation gain preview (20).
    [[nodiscard]] int order() const override { return 10; }

    void render(ui::HudContext& ctx) override {
        auto const& cfg = app::config();
        if (!ctx.hungerIconsValid || !ctx.state || !ctx.held
            || !core::shouldShowHungerPreview(cfg.showFoodValuesHudOverlay, *ctx.state, ctx.held->values)) {
            traceState("hidden");
            return;
        }
        auto const& food  = ctx.held->values;
        auto        icons = core::hungerPreviewIconList(ctx.state->hunger, food);
        if (icons.empty()) {
            traceState("none");
            return;
        }
        traceState(
            "hunger=" + std::to_string(ctx.state->hunger) + " food=" + std::to_string(food.hunger)
            + " icons=" + std::to_string(icons.front().index) + ".." + std::to_string(icons.back().index)
            + (food.rotten ? " rotten" : "") + (ctx.held->offhand ? " offhand" : "")
        );

        float alpha = ctx.flashAlpha;
        if (alpha <= 0) return;
        auto const& background = food.rotten ? EFFECT_BACKGROUND : BACKGROUND;
        for (auto const& icon : icons) {
            if (icon.index < 0 || icon.index >= core::ICONS_PER_ROW) continue;
            core::Rect r = ctx.hungerIcons[static_cast<std::size_t>(icon.index)];
            if (!cfg.showVanillaAnimationsOverlay) r.y -= ctx.hungerJitterY[static_cast<std::size_t>(icon.index)];
            // Very faint background, then the shank (AppleSkin drawHungerOverlay).
            ctx.painter.texture(background, r, alpha * 0.25f);
            ctx.painter.texture(spriteFor(icon.sprite), r, alpha);
        }
    }
};

HungerPreviewLayer gLayer;
BS_REGISTER_HUD_LAYER(gLayer);

} // namespace beetroot::features::hunger_preview

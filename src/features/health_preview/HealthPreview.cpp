// AppleSkin's HealthRestored overlay: while edible food is held, flash the hearts that eating it
// would restore (natural regeneration after eating + the food's Regeneration effect, Bedrock
// rules, see core/food/HealthPreview.h) on top of the vanilla hearts.
#include "core/food/HealthPreview.h"
#include "app/Mod.h"
#include "mod/Log.h"
#include "ui/gfx/Painter.h"
#include "ui/hud/HudHooks.h"

#include <cmath>
#include <optional>

namespace beetroot::features::health_preview {

namespace {

// Vanilla heart sprites (resource pack paths). Hardcore worlds have their own full/half hearts
// but share the container.
ui::TextureRef const HEART_CONTAINER = ui::TextureRef::pack("textures/ui/heart_background");
ui::TextureRef const HEART_FULL      = ui::TextureRef::pack("textures/ui/heart");
ui::TextureRef const HEART_HALF      = ui::TextureRef::pack("textures/ui/heart_half");
ui::TextureRef const HARDCORE_FULL   = ui::TextureRef::pack("textures/ui/hardcore/heart");
ui::TextureRef const HARDCORE_HALF   = ui::TextureRef::pack("textures/ui/hardcore/heart_half");

/// Log the preview once per change (no per-frame logging); used by the e2e scenario.
void logPlan(core::FoodState const& s, core::HealthPreviewPlan const& plan) {
    struct Key {
        bool  any                          = false;
        int   health                       = 0;
        int   newHealth                    = 0;
        int   first                        = 0;
        int   last                         = 0;
        bool  half                         = false;
        float maxHealth                    = 0;
        bool  operator==(Key const&) const = default;
    };
    static std::optional<Key> lastKey;
    Key                       key;
    if (!plan.icons.empty()) {
        key = {
            true,
            static_cast<int>(std::ceil(s.health)),
            static_cast<int>(std::ceil(plan.newHealth)),
            plan.icons.front().index,
            plan.icons.back().index,
            plan.icons.back().half,
            s.maxHealth
        };
    }
    if (lastKey == key) return;
    lastKey = key;
    if (!key.any) {
        log().info("Health preview: none");
        return;
    }
    log().info(
        "Health preview: health={} -> {} hearts={}..{}{} max={}",
        key.health,
        key.newHealth,
        key.first,
        key.last,
        key.half ? " (half)" : "",
        key.maxHealth
    );
}

class HealthPreviewLayer final : public ui::IHudLayer {
public:
    [[nodiscard]] ui::HudSlot slot() const override { return ui::HudSlot::AboveHearts; }

    void render(ui::HudContext& ctx) override {
        if (!ctx.state) return;
        auto const&             cfg = app::config();
        core::HealthPreviewPlan plan;
        if (cfg.showFoodHealthHudOverlay && ctx.held) plan = core::healthPreviewPlan(*ctx.state, ctx.held->values);
        logPlan(*ctx.state, plan);
        if (plan.icons.empty() || ctx.heartIcons.empty() || !(ctx.flashAlpha > 0)) return;

        bool        hardcore = ctx.state->hardcore;
        auto const& full     = hardcore ? HARDCORE_FULL : HEART_FULL;
        auto const& half     = hardcore ? HARDCORE_HALF : HEART_HALF;
        float       alpha    = ctx.flashAlpha;
        for (auto const& icon : plan.icons) {
            auto i = static_cast<std::size_t>(icon.index);
            if (icon.index < 0 || i >= ctx.heartIcons.size()) continue;
            auto rect = ctx.heartIcons[i];
            // The rects include vanilla's low-health shake; follow it only with
            // showVanillaAnimationsOverlay (AppleSkin), else draw at the resting position.
            if (!cfg.showVanillaAnimationsOverlay && i < ctx.heartJitterY.size()) rect.y -= ctx.heartJitterY[i];
            // Very faint container, then the heart (AppleSkin drawHealthOverlay).
            ctx.painter.texture(HEART_CONTAINER, rect, alpha * 0.25f);
            ctx.painter.texture(icon.half ? half : full, rect, alpha);
        }
    }
} gLayer;
BS_REGISTER_HUD_LAYER(gLayer);

} // namespace

} // namespace beetroot::features::health_preview

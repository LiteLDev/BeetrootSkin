// Scaffold verification aids, active only when bin/BeetrootSkin/debug_draw exists (the e2e
// smoke scenario creates it). They prove the contracts end to end:
// - a HUD layer that outlines every hunger/heart icon rect and draws AppleSkin's saturation
//   sprite from the mod-owned icons.png over the hunger bar;
// - a tooltip extension that fills its reserved area and logs the hovered item's food values.
#include "app/Mod.h"
#include "game/FoodValues.h"
#include "mod/Log.h"
#include "ui/gfx/Painter.h"
#include "ui/hud/HudHooks.h"
#include "ui/tooltip/TooltipHooks.h"

#include "mc/world/item/ItemStackBase.h"

#include <format>
#include <string>

namespace beetroot::debug {

namespace {

std::string describe(core::FoodValues const& f) {
    std::string s = std::format(
        "hunger={} saturation={:.2f} (modifier {:.2f}) canAlwaysEat={} rotten={} effects=[",
        f.hunger,
        f.saturation,
        f.saturationModifier,
        f.canAlwaysEat,
        f.rotten
    );
    for (auto const& e : f.effects) {
        s += std::format("{}#{} amp={} {}t p={:.2f}; ", e.name, e.id, e.amplifier, e.durationTicks, e.chance);
    }
    return s + "]";
}

class DebugHudLayer final : public ui::IHudLayer {
public:
    [[nodiscard]] ui::HudSlot slot() const override { return ui::HudSlot::AboveHunger; }

    void render(ui::HudContext& ctx) override {
        if (!app::debugDraw() || !ctx.hungerIconsValid) return;
        auto& p = ctx.painter;
        for (int i = 0; i < core::ICONS_PER_ROW; ++i) {
            auto const& r = ctx.hungerIcons[static_cast<std::size_t>(i)];
            // AppleSkin full saturation outline (u=27, v=0) from the mod-owned icons.png.
            p.texture(
                ui::textures::ICONS,
                r,
                {27, 0, 9, 9},
                core::sprites::ICONS_TEXTURE_SIZE,
                core::sprites::ICONS_TEXTURE_SIZE
            );
            p.outlineRect(
                r.inset(-1),
                i == 0 ? ui::Color::rgb(0xFF0000) : ui::Color::rgb(0x00FFFF, 0.8f),
                0.5f,
                ui::Painter::Layer::Foreground
            );
        }
        // Exhaustion strip from icons.png, right-aligned under the bar.
        auto const& right = ctx.hungerIcons[0];
        p.texture(
            ui::textures::ICONS,
            {right.right() - 81, right.bottom() + 1, 81, 9},
            {0, core::sprites::EXHAUSTION_BAR_V, 81, 9},
            core::sprites::ICONS_TEXTURE_SIZE,
            core::sprites::ICONS_TEXTURE_SIZE,
            ui::palette::WHITE.withAlpha(0.75f)
        );
        p.text(
            std::to_string(ctx.state ? ctx.state->hunger : -1),
            right.right() + 2,
            right.y,
            ui::Color::rgb(0xFFFF00),
            0.75f
        );

        std::string held = ctx.held ? describe(ctx.held->values) : std::string{"none"};
        if (held != mLastHeld) {
            mLastHeld = held;
            log().info("Debug: held food {}{}", held, ctx.held && ctx.held->offhand ? " (offhand)" : "");
        }
        if (!mLoggedTexture && p.textureReady(ui::textures::ICONS)) {
            mLoggedTexture = true;
            log().info("Debug: mod texture icons.png is ready and drawn at the hunger bar");
        }
    }

private:
    std::string mLastHeld;
    bool        mLoggedTexture = false;
} gHudLayer;
BS_REGISTER_HUD_LAYER(gHudLayer);

class DebugHeartsLayer final : public ui::IHudLayer {
public:
    [[nodiscard]] ui::HudSlot slot() const override { return ui::HudSlot::AboveHearts; }
    void                      render(ui::HudContext& ctx) override {
        if (!app::debugDraw()) return;
        for (std::size_t i = 0; i < ctx.heartIcons.size(); ++i) {
            ctx.painter.outlineRect(
                ctx.heartIcons[i].inset(-1),
                i == 0 ? ui::Color::rgb(0xFF0000) : ui::Color::rgb(0x00FF00, 0.8f),
                0.5f,
                ui::Painter::Layer::Foreground
            );
        }
    }
} gHeartsLayer;
BS_REGISTER_HUD_LAYER(gHeartsLayer);

class DebugTooltip final : public ui::ITooltipExtension {
public:
    [[nodiscard]] float extraHeight(ItemStackBase const& stack) override {
        if (!app::debugDraw()) return 0;
        auto food = game::getFoodValues(stack);
        auto desc = food ? describe(*food) : std::string{"not food"};
        if (desc != mLast) {
            mLast = desc;
            log().info("Debug: hovered {} -> {}", stack.getTypeName(), desc);
        }
        return 10;
    }
    [[nodiscard]] float extraWidth(ItemStackBase const& /*stack*/) override { return 100; }

    void render(ui::TooltipContext& ctx) override {
        ctx.painter.fillRect(ctx.area, ui::Color::rgb(0xFF00FF, 0.6f), ui::Painter::Layer::Foreground);
        ctx.painter.outlineRect(ctx.box, ui::Color::rgb(0x00FF00), 0.5f, ui::Painter::Layer::Foreground);
        ctx.painter.outlineRect(ctx.text, ui::Color::rgb(0xFFFF00), 0.5f, ui::Painter::Layer::Foreground);
        ctx.painter.texture(ui::textures::TOOLTIP_HUNGER_OUTLINE, {ctx.area.x + 1, ctx.area.y + 0.5f, 9, 9});
        ctx.painter.text("debug", ctx.area.x + 12, ctx.area.y + 1, ui::palette::TEXT_LIGHT, 0.75f);
    }

private:
    std::string mLast;
} gTooltip;
BS_REGISTER_TOOLTIP_EXTENSION(gTooltip);

} // namespace

} // namespace beetroot::debug

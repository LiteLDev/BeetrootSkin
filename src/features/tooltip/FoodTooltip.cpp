// AppleSkin's TooltipOverlayHandler: hunger and saturation icons in food item tooltips.
//
// Shown for edible items when (showFoodValuesInTooltip && Shift is held) ||
// showFoodValuesInTooltipAlways. Layout and sprite selection live in core/food/TooltipLayout;
// this file only maps them onto Bedrock textures.
#include "app/Mod.h"
#include "core/food/FoodMath.h"
#include "core/food/TooltipLayout.h"
#include "game/FoodValues.h"
#include "ui/Keys.h"
#include "ui/gfx/Painter.h"
#include "ui/tooltip/TooltipHooks.h"

#include <optional>
#include <utility>

namespace beetroot::features::tooltip {

namespace {

namespace layout = core::tooltip;

// Vanilla HUD hunger sprites (resource pack paths). Like AppleSkin, the background is always the
// plain empty shank and rotten food uses the hunger-effect (green) shanks.
ui::TextureRef const HUNGER_BACKGROUND  = ui::TextureRef::pack("textures/ui/hunger_background");
ui::TextureRef const HUNGER_FULL        = ui::TextureRef::pack("textures/ui/hunger_full");
ui::TextureRef const HUNGER_HALF        = ui::TextureRef::pack("textures/ui/hunger_half");
ui::TextureRef const HUNGER_ROTTEN_FULL = ui::TextureRef::pack("textures/ui/hunger_effect_full");
ui::TextureRef const HUNGER_ROTTEN_HALF = ui::TextureRef::pack("textures/ui/hunger_effect_half");

ui::TextureRef const& shank(bool rotten, bool half) {
    if (rotten) return half ? HUNGER_ROTTEN_HALF : HUNGER_ROTTEN_FULL;
    return half ? HUNGER_HALF : HUNGER_FULL;
}

bool enabled() {
    auto const& cfg = app::config();
    return (cfg.showFoodValuesInTooltip && ui::isShiftDown()) || cfg.showFoodValuesInTooltipAlways;
}

struct Food {
    bool                      rotten = false;
    layout::FoodTooltipLayout layout;
};

/// Rotten flag and layout of `stack` if the tooltip should show food values.
std::optional<Food> query(ItemStackBase const& stack) {
    if (!enabled()) return std::nullopt;
    auto values = game::getFoodValues(stack);
    if (!values) return std::nullopt;
    // Bedrock has no player-dependent food values: default == modified.
    auto l = layout::layoutFoodTooltip(values->hunger, values->saturation, values->hunger, values->saturation);
    if (!l.visible()) return std::nullopt;
    return Food{values->rotten, std::move(l)};
}

class FoodTooltipExtension final : public ui::ITooltipExtension {
public:
    // The tooltip plumbing calls extraHeight() first in every draw, then extraWidth() and render()
    // for the same stack: decide once per draw (Shift state, food values) so the reserved box and
    // the drawing always agree.
    float extraHeight(ItemStackBase const& stack) override {
        mFor    = &stack;
        mCached = query(stack);
        return mCached ? static_cast<float>(layout::FoodTooltipLayout::height()) : 0.f;
    }

    float extraWidth(ItemStackBase const& stack) override {
        auto const* food = get(stack);
        return food ? static_cast<float>(food->layout.width()) : 0.f;
    }

    void render(ui::TooltipContext& ctx) override {
        auto const* food = get(ctx.stack);
        if (!food) return;
        auto const&     l      = food->layout;
        auto&           p      = ctx.painter;
        float const     x0     = ctx.area.x;
        float const     y0     = ctx.area.y;
        bool const      rotten = food->rotten;
        auto const      fg     = ui::Painter::Layer::Foreground;
        constexpr float hs     = layout::HUNGER_ICON_SIZE;
        constexpr float ss     = layout::SATURATION_ICON_SIZE;

        // Hunger row: background, tinted outline, default value (faint), modified value (the
        // Painter keeps the submission order and splits its batches per texture).
        for (auto const& icon : l.hunger) {
            core::Rect dst{x0 + static_cast<float>(icon.x), y0, hs, hs};
            p.texture(HUNGER_BACKGROUND, dst, 1.f, fg);
            if (icon.outline != layout::Outline::Normal) {
                auto c = layout::outlineColor(icon.outline);
                p.texture(ui::textures::TOOLTIP_HUNGER_OUTLINE, dst, {0, 0, hs, hs}, hs, hs, {c.r, c.g, c.b, c.a}, fg);
            }
            p.texture(shank(rotten, icon.defaultHalf), dst, 0.25f, fg);
            if (icon.showModified) p.texture(shank(rotten, icon.modifiedHalf), dst, 1.f, fg);
        }

        // Saturation row.
        constexpr float t = core::sprites::ICONS_TEXTURE_SIZE;
        for (auto const& icon : l.saturation) {
            core::Rect dst{x0 + static_cast<float>(icon.x), y0 + layout::SATURATION_ROW_Y, ss, ss};
            core::Rect src{static_cast<float>(icon.u), static_cast<float>(icon.v), ss, ss};
            p.texture(ui::textures::ICONS, dst, src, t, t, ui::palette::WHITE.withAlpha(icon.faded ? 0.5f : 1.f), fg);
        }

        // "xN" after a single icon when there are more than 10 bars (or no saturation).
        auto const textColor = ui::Color::rgb(layout::OVERFLOW_TEXT_ARGB & 0xFFFFFF, 1.f);
        for (auto const* text : {&l.hungerText, &l.saturationText}) {
            if (text->text.empty()) continue;
            p.text(text->text, x0 + text->x, y0 + text->y, textColor, layout::OVERFLOW_TEXT_SCALE);
        }
    }

private:
    [[nodiscard]] Food const* get(ItemStackBase const& stack) {
        if (mFor != &stack) {
            // Not preceded by extraHeight() for this stack (should not happen): decide now.
            mFor    = &stack;
            mCached = query(stack);
        }
        return mCached ? &*mCached : nullptr;
    }

    // Only compared, never dereferenced; refreshed by every extraHeight() call.
    ItemStackBase const* mFor = nullptr;
    std::optional<Food>  mCached;
};

FoodTooltipExtension gExtension;
BS_REGISTER_TOOLTIP_EXTENSION(gExtension);

} // namespace

} // namespace beetroot::features::tooltip

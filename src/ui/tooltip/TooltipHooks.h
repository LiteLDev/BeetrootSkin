#pragma once

// Contract for adding content below the text of vanilla item tooltips (the hover text shown
// over container slots).
//
// ui/tooltip/TooltipHooks.cpp tracks the hovered container slot (ContainerScreenController /
// CraftingScreenController hover hooks) and hooks HoverTextRenderer::render. When the hovered
// item has extensions with extraHeight() > 0 it enlarges the vanilla tooltip box for that draw
// (height by the sum of the heights, width to the largest extraWidth()), lets vanilla draw the
// box and text, then calls each extension's render() with its reserved rectangle, stacked top to
// bottom in ascending order().
//
//     class MyExt final : public beetroot::ui::ITooltipExtension {
//     public:
//         float extraHeight(ItemStackBase const& s) override { return isFood(s) ? 10 : 0; }
//         void  render(TooltipContext& ctx) override { ctx.painter.fillRect(ctx.area, ...); }
//     } gMyExt;
//     BS_REGISTER_TOOLTIP_EXTENSION(gMyExt);

#include "core/geom/Rect.h"

#include <vector>

class ItemStackBase;

namespace beetroot::ui {

class Painter;

/// Per-draw information for tooltip extensions. GUI units. Only valid during render().
struct TooltipContext {
    Painter&             painter;
    ItemStackBase const& stack; ///< The hovered item (a copy owned by the tooltip plumbing).
    core::Rect           box;   ///< Whole tooltip box as drawn (vanilla part + all reserved areas).
    core::Rect           text;  ///< Content rect of the vanilla text (inside the box padding).
    core::Rect           area;  ///< This extension's reserved rect: content width x extraHeight().
};

/// Something drawn inside item tooltips. Registered once at static-init time; lives for the
/// whole session. All calls happen on the client thread during UI rendering.
class ITooltipExtension {
public:
    virtual ~ITooltipExtension() = default;

    /// Height (GUI units) to reserve below the vanilla text for this item; 0 = not shown.
    [[nodiscard]] virtual float extraHeight(ItemStackBase const& stack) = 0;
    /// Minimum content width needed (the box is widened if narrower); 0 = no requirement.
    [[nodiscard]] virtual float extraWidth(ItemStackBase const& /*stack*/) { return 0; }
    /// Extensions are stacked in ascending order().
    [[nodiscard]] virtual int order() const { return 0; }
    /// Draw into ctx.area (called after the vanilla tooltip was drawn).
    virtual void render(TooltipContext& ctx) = 0;
};

/// All registered extensions (registration order). Static-init safe.
[[nodiscard]] std::vector<ITooltipExtension*>& tooltipExtensions();

struct TooltipExtensionRegistrar {
    explicit TooltipExtensionRegistrar(ITooltipExtension& ext) { tooltipExtensions().push_back(&ext); }
};

/// The item in the hovered vanilla container slot, or nullptr (client thread, valid until the
/// next frame).
[[nodiscard]] ItemStackBase const* hoveredSlotItem();

} // namespace beetroot::ui

#define BS_TT_DETAIL_CONCAT2(a, b) a##b
#define BS_TT_DETAIL_CONCAT(a, b)  BS_TT_DETAIL_CONCAT2(a, b)
/// Register an ITooltipExtension instance (an object with static storage duration).
#define BS_REGISTER_TOOLTIP_EXTENSION(obj)                                                                             \
    [[maybe_unused]] static ::beetroot::ui::TooltipExtensionRegistrar BS_TT_DETAIL_CONCAT(                             \
        bsTooltipExtRegistrar_,                                                                                        \
        __LINE__                                                                                                       \
    ) {                                                                                                                \
        obj                                                                                                            \
    }

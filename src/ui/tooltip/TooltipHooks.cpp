// Tooltip plumbing: which container slot is hovered, and room for ITooltipExtensions inside the
// vanilla hover text box. Everything here runs on the client thread.
#include "ui/tooltip/TooltipHooks.h"

#include "app/Module.h"
#include "mod/Log.h"
#include "ui/gfx/Painter.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/memory/Hook.h"

#include "mc/client/gui/ViewRequest.h"
#include "mc/client/gui/components/NinePatchLayer.h"
#include "mc/client/gui/controls/UIControl.h"
#include "mc/client/gui/controls/renderers/HoverTextRenderer.h"
#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#include "mc/client/gui/screens/controllers/CraftingScreenController.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/ItemStackBase.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>

namespace beetroot::ui {

namespace tooltip_detail {

// --- Hovered slot ----------------------------------------------------------------------------

struct HoveredSlot {
    /// Identity of the controller that reported the hover. Only compared, never dereferenced.
    std::uintptr_t             owner = 0;
    std::string                collection;
    int                        index = -1;
    std::unique_ptr<ItemStack> item; ///< Copy of the slot's visual stack (null if empty).
};

// Leaked on purpose: never run game destructors during static destruction.
HoveredSlot& gHovered = *new HoveredSlot;

void readSlot(ContainerScreenController const& controller) {
    auto const& visual = controller._getVisualItemStack(gHovered.collection, gHovered.index);
    if (visual.isNull()) {
        gHovered.item.reset();
        return;
    }
    if (!gHovered.item) gHovered.item = std::make_unique<ItemStack>();
    static_cast<ItemStackBase&>(*gHovered.item) = visual;
}

void clearHover() {
    gHovered.owner = 0;
    gHovered.collection.clear();
    gHovered.index = -1;
    gHovered.item.reset();
}

void slotHovered(ContainerScreenController const& controller, std::string const& name, int index) {
    gHovered.owner      = reinterpret_cast<std::uintptr_t>(&controller);
    gHovered.collection = name;
    gHovered.index      = index;
    readSlot(controller);
}

void slotUnhovered(ContainerScreenController const& controller, std::string const& name, int index) {
    // Hover/unhover of neighbouring slots may arrive in either order; only clear our slot.
    if (gHovered.owner != reinterpret_cast<std::uintptr_t>(&controller) || gHovered.index != index
        || gHovered.collection != name)
        return;
    clearHover();
}

void screenTicked(ContainerScreenController const& controller) {
    if (gHovered.owner == reinterpret_cast<std::uintptr_t>(&controller)) readSlot(controller);
}

LL_TYPE_INSTANCE_HOOK(
    ContainerSlotHoveredHook,
    ll::memory::HookPriority::Normal,
    ContainerScreenController,
    &ContainerScreenController::$_onContainerSlotHovered,
    ::ui::ViewRequest,
    std::string const& collectionName,
    int                index
) {
    auto result = origin(collectionName, index);
    slotHovered(*this, collectionName, index);
    return result;
}

// The survival inventory and crafting table are CraftingScreenControllers, whose override does
// not necessarily reach the base implementation.
LL_TYPE_INSTANCE_HOOK(
    CraftingSlotHoveredHook,
    ll::memory::HookPriority::Normal,
    CraftingScreenController,
    &CraftingScreenController::$_onContainerSlotHovered,
    ::ui::ViewRequest,
    std::string const& collectionName,
    int                index
) {
    auto result = origin(collectionName, index);
    slotHovered(*this, collectionName, index);
    return result;
}

LL_TYPE_INSTANCE_HOOK(
    ContainerSlotUnhoveredHook,
    ll::memory::HookPriority::Normal,
    ContainerScreenController,
    &ContainerScreenController::$_onContainerSlotUnhovered,
    ::ui::ViewRequest,
    std::string const& collectionName,
    int                index
) {
    auto result = origin(collectionName, index);
    slotUnhovered(*this, collectionName, index);
    return result;
}

LL_TYPE_INSTANCE_HOOK(
    ContainerTickHook,
    ll::memory::HookPriority::Normal,
    ContainerScreenController,
    &ContainerScreenController::$tick,
    ::ui::DirtyFlag
) {
    auto result = origin();
    screenTicked(*this);
    return result;
}

LL_TYPE_INSTANCE_HOOK(
    CraftingTickHook,
    ll::memory::HookPriority::Normal,
    CraftingScreenController,
    &CraftingScreenController::$tick,
    ::ui::DirtyFlag
) {
    auto result = origin();
    screenTicked(*this);
    return result;
}

LL_TYPE_INSTANCE_HOOK(
    ContainerLeaveHook,
    ll::memory::HookPriority::Normal,
    ContainerScreenController,
    &ContainerScreenController::$onLeave,
    void
) {
    if (gHovered.owner == reinterpret_cast<std::uintptr_t>(this)) clearHover();
    origin();
}

// --- Tooltip box -----------------------------------------------------------------------------

/// Padding between the vanilla hover box edge and its text (GUI units).
constexpr float BOX_PADDING = 4;
/// Gap between the vanilla text and the first extension, and between extensions.
constexpr float GAP = 1;

struct Planned {
    ITooltipExtension* ext;
    float              height;
};

void logOnce(std::string msg) {
    static std::string last;
    if (msg == last) return;
    last = msg;
    log().error("Tooltip extension failed: {}", msg);
}

LL_TYPE_INSTANCE_HOOK(
    HoverTextRenderHook,
    ll::memory::HookPriority::Normal,
    HoverTextRenderer,
    &HoverTextRenderer::$render,
    void,
    ::MinecraftUIRenderContext& ctx,
    ::IClientInstance&          client,
    ::UIControl&                owner,
    int                         pass
) {
    auto& extensions = tooltipExtensions();
    if (extensions.empty() || !gHovered.item || mContent->empty()) return origin(ctx, client, owner, pass);

    ItemStack const&     stack = *gHovered.item;
    std::vector<Planned> planned;
    float                extraH = 0, extraW = 0;
    try {
        for (auto* ext : extensions) {
            float h = ext->extraHeight(stack);
            if (!(h > 0)) continue;
            planned.push_back({ext, h});
            extraH += h + GAP;
            extraW  = std::max(extraW, ext->extraWidth(stack));
        }
    } catch (std::exception const& e) {
        logOnce(e.what());
        planned.clear();
    }
    if (planned.empty()) return origin(ctx, client, owner, pass);
    std::stable_sort(planned.begin(), planned.end(), [](auto const& a, auto const& b) {
        return a.ext->order() < b.ext->order();
    });

    // Make room for this draw only. The vanilla box is a NinePatchLayer built in update() for
    // mBoxDimensions and drawn at mCursorPosition + mOffset; stretch its cached quads (middle
    // row/column grow, last row/column move) and restore everything after the draw.
    glm::vec2 const vanillaDims = mBoxDimensions;
    glm::vec2 const vanillaPos  = mCursorPosition;
    glm::vec2 const offset      = mOffset;
    float const     growX       = std::max(0.f, extraW + 2 * BOX_PADDING - vanillaDims.x);
    float const     growY       = extraH;
    glm::vec2       pos         = vanillaPos;
    // Keep the enlarged box on screen: move it up/left if it would leave the screen.
    {
        Painter probe(ctx);
        auto    screen = probe.screen();
        float   bottom = pos.y + offset.y + vanillaDims.y + growY;
        float   right  = pos.x + offset.x + vanillaDims.x + growX;
        if (bottom > screen.bottom()) pos.y -= bottom - screen.bottom();
        if (right > screen.right()) pos.x -= right - screen.right();
    }

    NinePatchLayer* layer = mLayer.get();
    using Quads           = std::array<NinePatchLayer::CachedQuad, 9>;
    Quads saved{};
    float savedW = 0, savedH = 0;
    if (layer) {
        savedW = layer->w;
        savedH = layer->h;
        for (std::size_t i = 0; i < 9; ++i) {
            auto& q  = layer->quads[i].get();
            saved[i] = q;
            // Rows/columns by position relative to the box center.
            if (q.y0 > savedH / 2) q.y0 += growY, q.y1 += growY; // bottom row
            else if (q.y1 > savedH / 2) q.y1 += growY;           // middle row
            if (q.x0 > savedW / 2) q.x0 += growX, q.x1 += growX; // right column
            else if (q.x1 > savedW / 2) q.x1 += growX;           // middle column
        }
        layer->w = savedW + growX;
        layer->h = savedH + growY;
    }
    mCursorPosition = pos;
    origin(ctx, client, owner, pass);
    mCursorPosition = vanillaPos;
    if (layer) {
        for (std::size_t i = 0; i < 9; ++i) layer->quads[i].get() = saved[i];
        layer->w = savedW;
        layer->h = savedH;
    }

    core::Rect box{pos.x + offset.x, pos.y + offset.y, vanillaDims.x + growX, vanillaDims.y + growY};
    core::Rect text{box.x + BOX_PADDING, box.y + BOX_PADDING, box.w - 2 * BOX_PADDING, vanillaDims.y - 2 * BOX_PADDING};
    float      y = text.bottom() + GAP;
    Painter    painter(ctx);
    for (auto const& p : planned) {
        TooltipContext tc{
            painter,
            stack,
            box,
            text,
            {text.x, y, text.w, p.height}
        };
        try {
            p.ext->render(tc);
            painter.flush();
        } catch (std::exception const& e) {
            logOnce(e.what());
        }
        y += p.height + GAP;
    }

    static glm::vec2 lastDims{-1, -1};
    if (vanillaDims != lastDims) {
        lastDims = vanillaDims;
        log().info(
            "Tooltip box: cursor {},{} offset {},{} dims {}x{} -> box {},{} {}x{}; nine-patch {} {}x{}",
            vanillaPos.x,
            vanillaPos.y,
            offset.x,
            offset.y,
            vanillaDims.x,
            vanillaDims.y,
            box.x,
            box.y,
            box.w,
            box.h,
            layer != nullptr,
            savedW,
            savedH
        );
    }
}

class TooltipModule final : public app::Module {
public:
    [[nodiscard]] char const* name() const override { return "tooltip-hooks"; }
    [[nodiscard]] int         order() const override { return -50; }

    void install() override {
        ContainerSlotHoveredHook::hook();
        CraftingSlotHoveredHook::hook();
        ContainerSlotUnhoveredHook::hook();
        ContainerTickHook::hook();
        CraftingTickHook::hook();
        ContainerLeaveHook::hook();
        HoverTextRenderHook::hook();
        mExit = ll::event::EventBus::getInstance().emplaceListener<ll::event::ClientExitLevelEvent>(
            [](ll::event::ClientExitLevelEvent&) { clearHover(); }
        );
    }

    void uninstall() override {
        if (mExit) ll::event::EventBus::getInstance().removeListener(mExit);
        mExit.reset();
        HoverTextRenderHook::unhook();
        ContainerLeaveHook::unhook();
        CraftingTickHook::unhook();
        ContainerTickHook::unhook();
        ContainerSlotUnhoveredHook::unhook();
        CraftingSlotHoveredHook::unhook();
        ContainerSlotHoveredHook::unhook();
        clearHover();
    }

private:
    ll::event::ListenerPtr mExit;
} gModule;
BS_REGISTER_MODULE(gModule);

} // namespace tooltip_detail

std::vector<ITooltipExtension*>& tooltipExtensions() {
    static std::vector<ITooltipExtension*> instance;
    return instance;
}

ItemStackBase const* hoveredSlotItem() { return tooltip_detail::gHovered.item.get(); }

} // namespace beetroot::ui

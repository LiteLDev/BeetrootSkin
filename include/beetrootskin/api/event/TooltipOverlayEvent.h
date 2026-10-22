#pragma once

// AppleSkin's TooltipOverlayEvent: posted (client thread) for the food values BeetrootSkin adds
// to item tooltips. Listen to the concrete (final) classes:
//
//     bus.emplaceListener<beetroot::api::TooltipOverlayEvent::Pre>([](auto& ev) {
//         if (ev.itemStack().getTypeName() == "minecraft:apple") ev.cancel(); // no food values
//     });
//
// Pre is posted when BeetrootSkin decides to show food values for the hovered item (after
// FoodValuesEvent, only when tooltips are enabled in the config and Shift is held / "always"
// is on); cancelling it removes them from the tooltip, including the reserved space. Render is
// posted right before they are drawn; cancelling it keeps the space but draws nothing (draw your
// own instead), and x()/y() may be changed to move them.

#include "beetrootskin/api/FoodValues.h"

#include "ll/api/event/Cancellable.h"
#include "ll/api/event/Event.h"
#include "ll/api/event/EventId.h"

class ItemStackBase;
class MinecraftUIRenderContext;

namespace beetroot::api {

class TooltipOverlayEvent : public ll::event::Cancellable<ll::event::Event> {
public:
    class Pre;
    class Render;

    /// The hovered item (only valid during the call).
    [[nodiscard]] ItemStackBase const& itemStack() const { return mItemStack; }
    /// Values from the item and after FoodValuesEvent (what the tooltip shows; differing values
    /// are drawn with AppleSkin's outlines).
    [[nodiscard]] FoodValues const& defaultFood() const { return mDefaultFood; }
    [[nodiscard]] FoodValues const& modifiedFood() const { return mModifiedFood; }

protected:
    TooltipOverlayEvent(ItemStackBase const& itemStack, FoodValues const& defaultFood, FoodValues const& modifiedFood)
    : mItemStack(itemStack),
      mDefaultFood(defaultFood),
      mModifiedFood(modifiedFood) {}

private:
    ItemStackBase const& mItemStack;
    FoodValues           mDefaultFood;
    FoodValues           mModifiedFood;
};

/// If cancelled, the tooltip gets no food values at all.
class TooltipOverlayEvent::Pre final : public TooltipOverlayEvent {
public:
    static constexpr ll::event::EventIdView CustomEventId{"beetroot::api::TooltipOverlayEvent::Pre"};

    Pre(ItemStackBase const& itemStack, FoodValues const& defaultFood, FoodValues const& modifiedFood)
    : TooltipOverlayEvent(itemStack, defaultFood, modifiedFood) {}
};

/// If cancelled, the space for the food values stays reserved but nothing is drawn.
class TooltipOverlayEvent::Render final : public TooltipOverlayEvent {
public:
    static constexpr ll::event::EventIdView CustomEventId{"beetroot::api::TooltipOverlayEvent::Render"};

    Render(
        ItemStackBase const&      itemStack,
        float                     x,
        float                     y,
        MinecraftUIRenderContext& renderContext,
        FoodValues const&         defaultFood,
        FoodValues const&         modifiedFood
    )
    : TooltipOverlayEvent(itemStack, defaultFood, modifiedFood),
      mX(x),
      mY(y),
      mRenderContext(renderContext) {}

    /// Top-left corner of the food values (GUI units). Listeners may change it.
    [[nodiscard]] float& x() { return mX; }
    [[nodiscard]] float& y() { return mY; }
    [[nodiscard]] float  x() const { return mX; }
    [[nodiscard]] float  y() const { return mY; }
    /// The UI render context of the tooltip pass.
    [[nodiscard]] MinecraftUIRenderContext& renderContext() const { return mRenderContext; }

private:
    float                     mX;
    float                     mY;
    MinecraftUIRenderContext& mRenderContext;
};

} // namespace beetroot::api

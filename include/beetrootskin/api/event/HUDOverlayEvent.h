#pragma once

// AppleSkin's HUDOverlayEvent: posted (client thread, during the HUD render pass) right before
// BeetrootSkin draws one of its HUD overlays. Cancel an event to suppress that overlay for this
// draw; change x()/y() to move it. Listen to the concrete (final) classes:
//
//     bus.emplaceListener<beetroot::api::HUDOverlayEvent::Exhaustion>([](auto& ev) { ev.cancel(); });
//
// Coordinates are GUI units (screen pixels x GuiData::mInvGuiScale). For the hunger-bar events
// (Exhaustion, Saturation, HungerRestored) x is the RIGHT edge of the hunger bar and y its top
// (resting position, without vanilla's shake); for HealthRestored x is the LEFT edge of the
// hearts and y the top of their bottom row. Like AppleSkin, each event is posted whenever
// BeetrootSkin is about to draw that overlay (enabled in the config, its bar shown, food held for
// the "restored" previews), every frame, even if it ends up drawing nothing (e.g. saturation 0,
// or the off phase of the flashing previews).

#include "beetrootskin/api/FoodValues.h"

#include "ll/api/event/Cancellable.h"
#include "ll/api/event/Event.h"
#include "ll/api/event/EventId.h"

class ItemStackBase;
class MinecraftUIRenderContext;

namespace beetroot::api {

class HUDOverlayEvent : public ll::event::Cancellable<ll::event::Event> {
public:
    class Exhaustion;
    class Saturation;
    class HungerRestored;
    class HealthRestored;

    /// Where the overlay is drawn (GUI units, see above). Listeners may change them.
    [[nodiscard]] float& x() { return mX; }
    [[nodiscard]] float& y() { return mY; }
    [[nodiscard]] float  x() const { return mX; }
    [[nodiscard]] float  y() const { return mY; }
    /// The UI render context of the HUD pass (to draw something of your own).
    [[nodiscard]] MinecraftUIRenderContext& renderContext() const { return mRenderContext; }

protected:
    HUDOverlayEvent(float x, float y, MinecraftUIRenderContext& renderContext)
    : mX(x),
      mY(y),
      mRenderContext(renderContext) {}

private:
    float                     mX;
    float                     mY;
    MinecraftUIRenderContext& mRenderContext;
};

/// The exhaustion underlay under the hunger bar. If cancelled, it is not drawn.
class HUDOverlayEvent::Exhaustion final : public HUDOverlayEvent {
public:
    static constexpr ll::event::EventIdView CustomEventId{"beetroot::api::HUDOverlayEvent::Exhaustion"};

    Exhaustion(float exhaustion, float x, float y, MinecraftUIRenderContext& renderContext)
    : HUDOverlayEvent(x, y, renderContext),
      mExhaustion(exhaustion) {}

    /// Current exhaustion (normally 0..4; the client's own prediction can briefly exceed 4 until
    /// the server consumes saturation/hunger).
    [[nodiscard]] float exhaustion() const { return mExhaustion; }

private:
    float mExhaustion;
};

/// The saturation overlay on the hunger bar. If cancelled, neither the current saturation nor the
/// held food's saturation gain is drawn.
class HUDOverlayEvent::Saturation final : public HUDOverlayEvent {
public:
    static constexpr ll::event::EventIdView CustomEventId{"beetroot::api::HUDOverlayEvent::Saturation"};

    Saturation(float saturationLevel, float x, float y, MinecraftUIRenderContext& renderContext)
    : HUDOverlayEvent(x, y, renderContext),
      mSaturationLevel(saturationLevel) {}

    /// Current saturation.
    [[nodiscard]] float saturationLevel() const { return mSaturationLevel; }

private:
    float mSaturationLevel;
};

/// The flashing shanks the held food would restore. If cancelled, they are not drawn (the
/// saturation gain is part of the Saturation overlay).
class HUDOverlayEvent::HungerRestored final : public HUDOverlayEvent {
public:
    static constexpr ll::event::EventIdView CustomEventId{"beetroot::api::HUDOverlayEvent::HungerRestored"};

    HungerRestored(
        int                       currentFoodLevel,
        ItemStackBase const&      itemStack,
        FoodValues const&         foodValues,
        float                     x,
        float                     y,
        MinecraftUIRenderContext& renderContext
    )
    : HUDOverlayEvent(x, y, renderContext),
      mCurrentFoodLevel(currentFoodLevel),
      mItemStack(itemStack),
      mFoodValues(foodValues) {}

    /// Current hunger (0..20).
    [[nodiscard]] int currentFoodLevel() const { return mCurrentFoodLevel; }
    /// The held food (only valid during the call).
    [[nodiscard]] ItemStackBase const& itemStack() const { return mItemStack; }
    /// Its modified food values (after FoodValuesEvent).
    [[nodiscard]] FoodValues const& foodValues() const { return mFoodValues; }

private:
    int                  mCurrentFoodLevel;
    ItemStackBase const& mItemStack;
    FoodValues           mFoodValues;
};

/// The flashing hearts eating the held food would restore. Only posted when the estimate is
/// above the current health. If cancelled, they are not drawn.
class HUDOverlayEvent::HealthRestored final : public HUDOverlayEvent {
public:
    static constexpr ll::event::EventIdView CustomEventId{"beetroot::api::HUDOverlayEvent::HealthRestored"};

    HealthRestored(
        float                     modifiedHealth,
        ItemStackBase const&      itemStack,
        FoodValues const&         foodValues,
        float                     x,
        float                     y,
        MinecraftUIRenderContext& renderContext
    )
    : HUDOverlayEvent(x, y, renderContext),
      mModifiedHealth(modifiedHealth),
      mItemStack(itemStack),
      mFoodValues(foodValues) {}

    /// Estimated health after eating (capped at max health).
    [[nodiscard]] float modifiedHealth() const { return mModifiedHealth; }
    /// The held food (only valid during the call).
    [[nodiscard]] ItemStackBase const& itemStack() const { return mItemStack; }
    /// Its modified food values (after FoodValuesEvent).
    [[nodiscard]] FoodValues const& foodValues() const { return mFoodValues; }

private:
    float                mModifiedHealth;
    ItemStackBase const& mItemStack;
    FoodValues           mFoodValues;
};

} // namespace beetroot::api

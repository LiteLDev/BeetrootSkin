#pragma once

// AppleSkin's FoodValuesEvent: customize the hunger/saturation values BeetrootSkin shows for a
// food. Posted (client thread) whenever BeetrootSkin determines the food values of an item: for
// item tooltips and for the held food behind every HUD preview (hunger, saturation and health
// restored). Not cancellable.
//
//     auto& bus = ll::event::EventBus::getInstance();
//     auto  l   = bus.emplaceListener<beetroot::api::FoodValuesEvent>([](beetroot::api::FoodValuesEvent& ev) {
//         if (ev.itemStack().getTypeName() == "minecraft:bread") ev.modifiedFoodValues().hunger *= 2;
//     });

#include "beetrootskin/api/FoodValues.h"

#include "ll/api/event/Event.h"
#include "ll/api/event/EventId.h"

class ItemStackBase;
class Player;

namespace beetroot::api {

class FoodValuesEvent final : public ll::event::Event {
public:
    static constexpr ll::event::EventIdView CustomEventId{"beetroot::api::FoodValuesEvent"};

    FoodValuesEvent(
        Player*              player,
        ItemStackBase const& itemStack,
        FoodValues const&    defaultFoodValues,
        FoodValues const&    modifiedFoodValues
    )
    : mPlayer(player),
      mItemStack(itemStack),
      mDefault(defaultFoodValues),
      mModified(modifiedFoodValues) {}

    /// The local player (may be null, e.g. outside a world).
    [[nodiscard]] Player* player() const { return mPlayer; }
    /// The item whose values are determined (only valid during the call).
    [[nodiscard]] ItemStackBase const& itemStack() const { return mItemStack; }
    /// The item's own values. Changing them changes what tooltips show as the default (faint).
    [[nodiscard]] FoodValues&       defaultFoodValues() { return mDefault; }
    [[nodiscard]] FoodValues const& defaultFoodValues() const { return mDefault; }
    /// The values BeetrootSkin uses for every preview; starts equal to defaultFoodValues().
    [[nodiscard]] FoodValues&       modifiedFoodValues() { return mModified; }
    [[nodiscard]] FoodValues const& modifiedFoodValues() const { return mModified; }

private:
    Player*              mPlayer;
    ItemStackBase const& mItemStack;
    FoodValues           mDefault;
    FoodValues           mModified;
};

} // namespace beetroot::api

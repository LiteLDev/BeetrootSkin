#include "game/FoodValues.h"

#include "core/food/VanillaFoods.h"

#include "mc/world/item/FoodItemComponentLegacy.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStackBase.h"
#include "mc/world/item/components/IFoodItemComponent.h"

#include <memory>

namespace beetroot::game {

namespace {

/// Whether `food` (the component of `item`) is a FoodItemComponentLegacy, which carries the eat
/// effects. No RTTI and no usable function address (MCAPI pointers lead to LeviLamina's lazy
/// import stubs, not to the game code that vtables point at), so check the layout instead: a
/// legacy component starts with { vptr, Item& mOwner, int mNutrition, float mSaturationModifier }.
/// For the data-driven FoodItemComponent the IFoodItemComponent subobject is followed by other
/// data, which never matches the owner pointer and both values.
bool isLegacyFood(IFoodItemComponent const& food, Item const& item) {
    auto const& legacy = static_cast<FoodItemComponentLegacy const&>(food);
    return std::addressof(legacy.mOwner) == std::addressof(item) && legacy.mNutrition == food.getNutrition()
        && legacy.mSaturationModifier == food.getSaturationModifier();
}

} // namespace

std::optional<core::FoodValues> getFoodValues(ItemStackBase const& stack, Player const* /*player*/) {
    if (stack.isNull()) return std::nullopt;
    Item const* item = stack.mItem.get();
    if (!item) return std::nullopt;
    IFoodItemComponent const* food = item->getFood();
    if (!food) return std::nullopt;

    auto values = core::FoodValues::make(food->getNutrition(), food->getSaturationModifier(), food->canAlwaysEat());
    if (isLegacyFood(*food, *item)) {
        auto const& legacy = static_cast<FoodItemComponentLegacy const&>(*food);
        for (auto const& e : legacy.mEffects.get()) {
            core::FoodEffect fe;
            fe.id            = e.id;
            fe.amplifier     = e.amplifier;
            fe.durationTicks = e.duration.get().mValue;
            fe.chance        = e.chance;
            fe.name          = e.name.get();
            values.rotten    = values.rotten || core::isHarmfulEffect(fe.id);
            values.effects.push_back(std::move(fe));
        }
    }
    if (values.effects.empty()) {
        // Data-driven components (and legacy ones without effects) carry no effect list: use the
        // vanilla table.
        values.effects = core::vanillaFoodEffects(stack.getTypeName());
        values.rotten  = core::anyHarmful(values.effects);
    }
    return values;
}

} // namespace beetroot::game

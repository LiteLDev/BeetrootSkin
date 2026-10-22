#include "game/FoodValues.h"

#include "api/Post.h"
#include "core/food/ApiFoodValues.h"
#include "core/food/VanillaFoods.h"
#include "game/Game.h"
#include "mod/Log.h"

#include "beetrootskin/api/event/FoodValuesEvent.h"

#include "mc/client/player/LocalPlayer.h"

#include "mc/world/item/FoodItemComponentLegacy.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStackBase.h"
#include "mc/world/item/components/IFoodItemComponent.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace beetroot::game {

namespace {

/// Logs, per item, when FoodValuesEvent listeners change its values (once per change). Locked:
/// the exported api::queryFoodValues may be called from other mods on any thread.
void traceModified(ItemStackBase const& stack, api::FoodValues const& def, api::FoodValues const& mod) {
    static std::mutex                                                         mutex;
    static std::map<std::string, std::pair<api::FoodValues, api::FoodValues>> last;
    std::lock_guard                                                           lock{mutex};
    auto                                                                      name = stack.getTypeName();
    auto                                                                      it   = last.find(name);
    if (it == last.end()) {
        if (def == mod) return; // never modified so far: nothing to report
        it = last.emplace(name, std::pair{def, def}).first;
    }
    if (it->second == std::pair{def, mod}) return;
    it->second = {def, mod};
    log().info(
        "API: FoodValuesEvent {}: default hunger={} saturation={:.2f} -> modified hunger={} saturation={:.2f}{}",
        name,
        def.hunger,
        def.saturation,
        mod.hunger,
        mod.saturation,
        mod.canAlwaysEat ? " canAlwaysEat" : ""
    );
}

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

std::optional<core::FoodValues> getDefaultFoodValues(ItemStackBase const& stack) {
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

std::optional<QueriedFoodValues> queryFoodValues(ItemStackBase const& stack, Player* player) {
    auto values = getDefaultFoodValues(stack);
    if (!values) return std::nullopt;
    if (!player) player = currentPlayer();
    auto                 defaults = core::toApiFoodValues(*values);
    api::FoodValuesEvent ev{player, stack, defaults, defaults};
    api_impl::post(ev);
    traceModified(stack, ev.defaultFoodValues(), ev.modifiedFoodValues());
    return QueriedFoodValues{
        core::withApiFoodValues(*values, ev.defaultFoodValues()),
        core::withApiFoodValues(*values, ev.modifiedFoodValues())
    };
}

std::optional<core::FoodValues> getFoodValues(ItemStackBase const& stack, Player const* player) {
    // The event hands listeners a mutable Player (like AppleSkin); BeetrootSkin never mutates it.
    auto q = queryFoodValues(stack, const_cast<Player*>(player));
    if (!q) return std::nullopt;
    return std::move(q->modifiedValues);
}

} // namespace beetroot::game

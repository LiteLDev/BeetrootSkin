#pragma once

#include "core/food/FoodMath.h"

#include <optional>

class ItemStackBase;

namespace beetroot::game {

/// The local player's hunger/saturation/exhaustion/health and related flags, or nullopt when
/// not in a world. Client thread.
///
/// When this process hosts the world (single-player / LAN host) the hunger, saturation,
/// exhaustion and health values come from the integrated server's player (exact, snapshotted
/// on the server thread every server tick; FoodState::exact = true). Otherwise (remote server)
/// they come from the client's synced attributes.
[[nodiscard]] std::optional<core::FoodState> readFoodState();

/// Dev/test switch (features/sync): ignore the integrated server snapshot so readFoodState()
/// takes the remote-server (client attribute) path even in single-player. Any thread.
void               setForceClientSource(bool force);
[[nodiscard]] bool forceClientSource();

/// readFoodState() from the client's own attributes and level, ignoring the integrated server
/// (what a remote-server client sees). Client thread.
[[nodiscard]] std::optional<core::FoodState> readClientFoodState();
/// readFoodState() from the integrated server snapshot regardless of setForceClientSource();
/// nullopt when there is no current snapshot (remote server, not in a world). Only hunger,
/// saturation, exhaustion, health, peaceful and naturalRegen come from the server; the other
/// fields (effects, game mode, hardcore, riding) are the client's in either case. Client thread.
[[nodiscard]] std::optional<core::FoodState> readIntegratedServerFoodState();

/// Food the local player holds, as AppleSkin picks it.
struct HeldFood {
    core::FoodValues values;          ///< Modified values (after api::FoodValuesEvent).
    bool             offhand = false; ///< Taken from the offhand slot.
    /// The held stack in the player's inventory. Only valid during the current frame / tick (do
    /// not keep it); null when unknown.
    ItemStackBase const* stack = nullptr;
    /// The item's own values (before FoodValuesEvent listeners changed them).
    core::FoodValues defaultValues;
};
/// Main hand first; then the offhand if Config::showFoodValuesHudOverlayWhenOffhand.
/// nullopt when neither hand holds food or not in a world. Client thread.
[[nodiscard]] std::optional<HeldFood> heldFood();

} // namespace beetroot::game

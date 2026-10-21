#pragma once

#include "core/food/FoodMath.h"

#include <optional>

namespace beetroot::game {

/// The local player's hunger/saturation/exhaustion/health and related flags, or nullopt when
/// not in a world. Client thread.
///
/// When this process hosts the world (single-player / LAN host) the hunger, saturation,
/// exhaustion and health values come from the integrated server's player (exact, snapshotted
/// on the server thread every server tick; FoodState::exact = true). Otherwise (remote server)
/// they come from the client's synced attributes.
[[nodiscard]] std::optional<core::FoodState> readFoodState();

/// Food the local player holds, as AppleSkin picks it.
struct HeldFood {
    core::FoodValues values;
    bool             offhand = false; ///< Taken from the offhand slot.
};
/// Main hand first; then the offhand if Config::showFoodValuesHudOverlayWhenOffhand.
/// nullopt when neither hand holds food or not in a world. Client thread.
[[nodiscard]] std::optional<HeldFood> heldFood();

} // namespace beetroot::game

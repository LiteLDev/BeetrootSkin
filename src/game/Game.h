#pragma once

class ClientInstance;
class ClientLevel;
class LocalPlayer;
class Level;

namespace beetroot::game {

/// Current client instance, or nullptr before the game finished starting.
[[nodiscard]] ClientInstance* currentClient();
/// Current client level, or nullptr when not in a world.
[[nodiscard]] ClientLevel* currentLevel();
/// Local player, or nullptr when not in a world.
[[nodiscard]] LocalPlayer* currentPlayer();
/// The integrated server's level when this process hosts the world (single-player / LAN
/// host), else nullptr. Owned by the server thread: never read players from it on the client
/// thread; use game/PlayerFoodState.h, which snapshots on the server thread.
[[nodiscard]] Level* integratedServerLevel();

} // namespace beetroot::game

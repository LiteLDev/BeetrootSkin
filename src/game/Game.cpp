#include "game/Game.h"

#include "ll/api/service/Bedrock.h"
#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/multiplayer/ClientLevel.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/level/Level.h"

namespace beetroot::game {

ClientInstance* currentClient() { return ll::service::getClientInstance().as_ptr(); }

ClientLevel* currentLevel() {
    auto* client = currentClient();
    return client ? client->getLevel() : nullptr;
}

LocalPlayer* currentPlayer() {
    auto* client = currentClient();
    return client ? client->getLocalPlayer() : nullptr;
}

Level* integratedServerLevel() { return ll::service::getLevel().as_ptr(); }

} // namespace beetroot::game

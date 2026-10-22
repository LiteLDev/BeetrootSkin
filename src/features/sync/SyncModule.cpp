// Food-state accuracy and "sync" diagnostics (AppleSkin's SyncHandler equivalent).
//
// AppleSkin needs a packet to get saturation, exhaustion and the naturalRegeneration game rule
// to Java clients. BeetrootSkin is client-only, so instead this module watches what the server
// already sends and makes sure game::readFoodState() is right in every situation:
//
// - UpdateAttributesPacket (ClientNetworkHandler): which food attributes the server syncs to
//   the local player (logged once per attribute and session);
// - GameRulesChangedPacket / SetDifficultyPacket (LegacyClientNetworkHandler): the client
//   level's naturalregeneration / difficulty after the server changed them;
// - the food state source (integrated server / client attributes), logged on every change;
// - the dev switch `mods/BeetrootSkin/force_client_sync` (polled once per second, so a test can
//   toggle it in game): readFoodState() then ignores the integrated server, i.e. behaves like a
//   remote-server client, and the client and server values are logged side by side on change.

#include "app/Mod.h"
#include "app/Module.h"
#include "game/Game.h"
#include "game/PlayerFoodState.h"
#include "mod/Log.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/client/ClientJoinLevelEvent.h"
#include "ll/api/event/world/ClientLevelTickEvent.h"
#include "ll/api/memory/Hook.h"

#include "mc/client/network/ClientNetworkHandler.h"
#include "mc/client/network/LegacyClientNetworkHandler.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/network/packet/GameRulesChangedPacket.h"
#include "mc/network/packet/SetDifficultyPacket.h"
#include "mc/network/packet/UpdateAttributesPacket.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/attribute/Attribute.h"
#include "mc/world/attribute/AttributeData.h"
#include "mc/world/attribute/SharedAttributes.h"
#include "mc/world/level/storage/GameRule.h"

#include <array>
#include <cmath>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>

namespace beetroot::features::sync {
namespace {

constexpr char const* FORCE_MARKER = "force_client_sync";

// --- Client-thread state (reset on join/exit) -------------------------------------------------

enum class Source { None, IntegratedServer, Client, ClientForced };

char const* sourceName(Source s) {
    switch (s) {
    case Source::IntegratedServer:
        return "integrated-server";
    case Source::Client:
        return "client";
    case Source::ClientForced:
        return "client (forced by force_client_sync)";
    default:
        return "none";
    }
}

struct Session {
    std::array<bool, 4> attrSeen{}; // hunger, saturation, exhaustion, health
    int                 foodPackets = 0;
    Source              source      = Source::None;
    // Client level rules as last logged.
    std::tuple<bool, bool, bool> rules{};
    bool                         rulesLogged   = false;
    bool                         rulesMismatch = false;
    int                          mismatchTicks = 0;
    // Dev compare (forced mode).
    std::array<long, 8> compare{};
    bool                compareLogged = false;
};

Session gSession;
int     gTick = 0;

std::string_view const FOOD_ATTRS[4] = {
    "minecraft:player.hunger",
    "minecraft:player.saturation",
    "minecraft:player.exhaustion",
    "minecraft:health",
};

void onLocalPlayerAttributes(UpdateAttributesPacket const& packet) {
    bool food = false;
    for (auto const& a : *packet.mAttributeData) {
        auto const& name = a.mName->getString();
        for (std::size_t i = 0; i < 4; ++i) {
            if (name != FOOD_ATTRS[i]) continue;
            food = i < 3 || food;
            if (!gSession.attrSeen[i]) {
                gSession.attrSeen[i] = true;
                log().info("Sync: server syncs {} to the client (first value {:.3f})", name, a.mCurrentValue);
            }
        }
    }
    if (food) ++gSession.foodPackets;
}

// Logs the client level's difficulty/natural regeneration/hardcore whenever they change, and
// whether they disagree with the integrated server (for longer than a network round trip).
void checkRules(core::FoodState const& client, std::optional<core::FoodState> const& server) {
    auto rules = std::make_tuple(client.peaceful, client.naturalRegen, client.hardcore);
    if (!gSession.rulesLogged || rules != gSession.rules) {
        gSession.rulesLogged = true;
        gSession.rules       = rules;
        log().info(
            "Sync: client level peaceful={} naturalRegen={} hardcore={}",
            client.peaceful,
            client.naturalRegen,
            client.hardcore
        );
    }
    bool mismatch = server && (server->peaceful != client.peaceful || server->naturalRegen != client.naturalRegen);
    gSession.mismatchTicks = mismatch ? gSession.mismatchTicks + 1 : 0;
    bool report            = gSession.mismatchTicks > 40;
    if (report == gSession.rulesMismatch) return;
    gSession.rulesMismatch = report;
    if (report) {
        log().warn(
            "Sync: client level rules differ from the server: client peaceful={} naturalRegen={}, server "
            "peaceful={} naturalRegen={}",
            client.peaceful,
            client.naturalRegen,
            server->peaceful,
            server->naturalRegen
        );
    } else {
        log().info("Sync: client level rules match the server again");
    }
}

// readFoodState() is the server snapshot when there is one, unless forced to the client path.
void checkSource(std::optional<core::FoodState> const& client, std::optional<core::FoodState> const& server) {
    bool   forced = game::forceClientSource();
    Source s      = !client           ? Source::None
                  : server && !forced ? Source::IntegratedServer
                  : forced            ? Source::ClientForced
                                      : Source::Client;
    if (s == gSession.source) return;
    log().info("Sync: food state source {} -> {}", sourceName(gSession.source), sourceName(s));
    gSession.source = s;
}

// Forced mode only: client attribute values next to the integrated server's, on change.
void logCompare(core::FoodState const& client, core::FoodState const& server) {
    auto                q = [](float v) { return std::lround(v * 10); };
    std::array<long, 8> now{
        client.hunger,
        q(client.saturation),
        q(client.exhaustion),
        q(client.health),
        server.hunger,
        q(server.saturation),
        q(server.exhaustion),
        q(server.health)
    };
    if (gSession.compareLogged && now == gSession.compare) return;
    gSession.compareLogged = true;
    gSession.compare       = now;
    log().info(
        "Sync compare: client hunger={} saturation={:.2f} exhaustion={:.3f} health={:.1f} | server hunger={} "
        "saturation={:.2f} exhaustion={:.3f} health={:.1f}",
        client.hunger,
        client.saturation,
        client.exhaustion,
        client.health,
        server.hunger,
        server.saturation,
        server.exhaustion,
        server.health
    );
}

void pollForceMarker() {
    std::error_code ec;
    bool            force = std::filesystem::exists(app::modFile(FORCE_MARKER), ec);
    if (force == game::forceClientSource()) return;
    game::setForceClientSource(force);
    log().info("Sync: {} marker {}", FORCE_MARKER, force ? "found: using the client attribute path" : "removed");
}

void onClientTick() {
    if (++gTick % 20 == 0) pollForceMarker();
    auto client = game::readClientFoodState();
    auto server = game::readIntegratedServerFoodState();
    checkSource(client, server);
    if (!client) return;
    checkRules(*client, server);
    if (game::forceClientSource() && server) logCompare(*client, *server);
    else gSession.compareLogged = false;
}

} // namespace
} // namespace beetroot::features::sync

namespace beetroot::features::sync::hooks {

// Attribute updates for any actor; only the local player's matter.
LL_TYPE_INSTANCE_HOOK(
    UpdateAttributesHook,
    ll::memory::HookPriority::Normal,
    ClientNetworkHandler,
    &ClientNetworkHandler::$handle,
    void,
    NetworkIdentifier const&                source,
    std::shared_ptr<UpdateAttributesPacket> packet
) {
    try {
        auto* player = game::currentPlayer();
        if (packet && player && packet->mRuntimeId->rawID == player->getRuntimeID().rawID) {
            onLocalPlayerAttributes(*packet);
        }
    } catch (...) {}
    origin(source, std::move(packet));
}

LL_TYPE_INSTANCE_HOOK(
    GameRulesChangedHook,
    ll::memory::HookPriority::Normal,
    LegacyClientNetworkHandler,
    &LegacyClientNetworkHandler::$handle,
    void,
    NetworkIdentifier const&      source,
    GameRulesChangedPacket const& packet
) {
    origin(source, packet);
    try {
        // Rare (join, /gamerule): log the whole packet so the rule names/values are visible.
        std::string rules;
        for (auto const& rule : packet.mRuleData->mRules.get()) {
            if (!rules.empty()) rules += ", ";
            rules += rule.mName.get();
            if (auto const* b = std::get_if<bool>(&rule.mValue.get())) rules += *b ? "=true" : "=false";
            else if (auto const* i = std::get_if<int>(&rule.mValue.get())) rules += "=" + std::to_string(*i);
        }
        log().info("Sync: server sent game rules: {}", rules);
    } catch (...) {}
}

LL_TYPE_INSTANCE_HOOK(
    SetDifficultyHook,
    ll::memory::HookPriority::Normal,
    LegacyClientNetworkHandler,
    &LegacyClientNetworkHandler::$handle,
    void,
    NetworkIdentifier const&   source,
    SetDifficultyPacket const& packet
) {
    origin(source, packet);
    try {
        log().info("Sync: server sent difficulty {}", static_cast<int>(packet.mDifficulty));
    } catch (...) {}
}

} // namespace beetroot::features::sync::hooks

namespace beetroot::features::sync {
namespace {

class SyncModule final : public app::Module {
public:
    [[nodiscard]] char const* name() const override { return "sync"; }

    void install() override {
        log().info(
            "Sync: attributes marked syncable: hunger={} saturation={} exhaustion={}",
            static_cast<bool>(Player::HUNGER().mSyncable),
            static_cast<bool>(Player::SATURATION().mSyncable),
            static_cast<bool>(Player::EXHAUSTION().mSyncable)
        );
        hooks::UpdateAttributesHook::hook();
        hooks::GameRulesChangedHook::hook();
        hooks::SetDifficultyHook::hook();
        auto& bus = ll::event::EventBus::getInstance();
        mTick     = bus.emplaceListener<ll::event::ClientLevelTickEvent>([](ll::event::ClientLevelTickEvent&) {
            try {
                onClientTick();
            } catch (...) {}
        });
        // No session reset on join: leaving resets it, and on a remote server the client level
        // ticks and the first attribute/game-rule packets arrive before ClientJoinLevelEvent
        // (verified against a dedicated server), so a reset here would drop what was logged.
        mJoin = bus.emplaceListener<ll::event::ClientJoinLevelEvent>([](ll::event::ClientJoinLevelEvent&) {
            pollForceMarker();
        });
        mExit = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](ll::event::ClientExitLevelEvent&) {
            if (gSession.source != Source::None) {
                log().info(
                    "Sync: left level ({} food attribute updates received); food state source {} -> none",
                    gSession.foodPackets,
                    sourceName(gSession.source)
                );
            }
            gSession = {};
        });
    }

    void uninstall() override {
        hooks::UpdateAttributesHook::unhook();
        hooks::GameRulesChangedHook::unhook();
        hooks::SetDifficultyHook::unhook();
        auto& bus = ll::event::EventBus::getInstance();
        for (auto* l : {&mTick, &mJoin, &mExit}) {
            if (*l) bus.removeListener(*l);
            l->reset();
        }
        game::setForceClientSource(false);
    }

private:
    ll::event::ListenerPtr mTick, mJoin, mExit;
} gModule;
BS_REGISTER_MODULE(gModule);

} // namespace
} // namespace beetroot::features::sync

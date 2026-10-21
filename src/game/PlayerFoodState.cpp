#include "game/PlayerFoodState.h"

#include "app/Mod.h"
#include "app/Module.h"
#include "game/FoodValues.h"
#include "game/Game.h"
#include "mod/Log.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/client/ClientJoinLevelEvent.h"
#include "ll/api/event/world/ClientLevelTickEvent.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"

#include "mc/client/multiplayer/ClientLevel.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/shared_types/legacy/Difficulty.h"
#include "mc/legacy/ActorUniqueID.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/attribute/AttributeInstance.h"
#include "mc/world/attribute/AttributeInstanceConstRef.h"
#include "mc/world/attribute/SharedAttributes.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/storage/GameRuleId.h"
#include "mc/world/level/storage/GameRules.h"

#include "mc/world/level/storage/GameRule.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <variant>

namespace beetroot::game {

namespace {

constexpr int GAME_RULE_NATURAL_REGENERATION = 20; // GameRules::GameRulesIndex::DoNaturalRegeneration

float attributeValue(Actor const& actor, Attribute const& attribute, float fallback) {
    auto        ref = actor.getAttribute(attribute);
    auto const* p   = ref.mPtr;
    return p ? p->mCurrentValue : fallback;
}

float attributeMax(Actor const& actor, Attribute const& attribute, float fallback) {
    auto        ref = actor.getAttribute(attribute);
    auto const* p   = ref.mPtr;
    return p ? p->mCurrentMaxValue : fallback;
}

bool naturalRegen(Level const& level) {
    // By name first: the rule's position in GameRules::mGameRules is not guaranteed to match
    // the SDK's GameRulesIndex for this game version.
    auto const& rules = level.getGameRules();
    for (auto const& rule : rules.mGameRules.get()) {
        auto const& name = rule.mName.get();
        if (name.size() != 19 || !std::equal(name.begin(), name.end(), "naturalregeneration", [](char a, char b) {
                return std::tolower(static_cast<unsigned char>(a)) == b;
            }))
            continue;
        if (auto const* v = std::get_if<bool>(&rule.mValue.get())) return *v;
        break;
    }
    GameRuleId id;
    id.mValue = GAME_RULE_NATURAL_REGENERATION;
    return rules.getBool(id, true);
}

// --- Integrated server snapshot -------------------------------------------------------------
// The client thread publishes the local player's unique id; the server thread (ServerLevelTick)
// copies that player's values into gServer. Nothing on the client thread ever touches the
// server level.
//
// Lifetime: every join and exit starts a new generation, and a snapshot is only used by the
// generation that requested it, so values never leak into the next world or onto a remote
// server (not even from a server tick that was in flight while the client left).

constexpr std::int64_t NO_PLAYER = INT64_MIN;

struct ServerSnapshot {
    std::int64_t                          id     = NO_PLAYER;
    std::uint64_t                         gen    = 0; // gGeneration when requested
    std::uint64_t                         seq    = 0; // gServerTickSeq of the tick that took it
    float                                 hunger = 0, saturation = 0, exhaustion = 0, health = 0, maxHealth = 0;
    bool                                  peaceful = false, naturalRegen = true;
    std::chrono::steady_clock::time_point at{};
};

std::atomic<std::int64_t>  gWantedId{NO_PLAYER};
std::atomic<std::uint64_t> gGeneration{1};
std::mutex                 gServerMutex;
ServerSnapshot             gServer;
std::atomic<bool>          gForceClient{false};
std::uint64_t              gServerTickSeq = 0; // server ticks seen (guarded by gServerMutex)

void onServerTick(Level& level) {
    std::uint64_t seq;
    {
        std::lock_guard lock{gServerMutex};
        seq = ++gServerTickSeq;
    }
    // Generation first: if the client leaves between the two loads, the id is tagged with the
    // old generation and the snapshot is rejected.
    auto gen = gGeneration.load();
    auto id  = gWantedId.load();
    if (id == NO_PLAYER) return;
    Player* p = level.getPlayer(ActorUniqueID{id});
    if (!p) return;
    ServerSnapshot s;
    s.id           = id;
    s.gen          = gen;
    s.seq          = seq;
    s.hunger       = attributeValue(*p, Player::HUNGER(), 20);
    s.saturation   = attributeValue(*p, Player::SATURATION(), 0);
    s.exhaustion   = attributeValue(*p, Player::EXHAUSTION(), 0);
    s.health       = attributeValue(*p, SharedAttributes::HEALTH(), 20);
    s.maxHealth    = attributeMax(*p, SharedAttributes::HEALTH(), 20);
    s.peaceful     = level.getDifficulty() == SharedTypes::Legacy::Difficulty::Peaceful;
    s.naturalRegen = naturalRegen(level);
    s.at           = std::chrono::steady_clock::now();
    std::lock_guard lock{gServerMutex};
    gServer = s;
}

std::optional<ServerSnapshot> freshServerSnapshot(std::int64_t id) {
    std::lock_guard lock{gServerMutex};
    if (id == NO_PLAYER || gServer.id != id || gServer.gen != gGeneration.load()) return std::nullopt;
    // Fresh, or taken by the latest server tick while the integrated server still exists: then
    // it is merely paused (pause menu, unfocused window) and the values cannot have changed. A
    // server that keeps ticking without finding the player makes the snapshot stale after 500 ms.
    bool fresh = std::chrono::steady_clock::now() - gServer.at <= std::chrono::milliseconds(500);
    if (!fresh && (gServer.seq != gServerTickSeq || !integratedServerLevel())) return std::nullopt;
    return gServer;
}

// Joining or leaving a world: start a new generation and drop the old values.
void resetServerSnapshot() {
    gWantedId = NO_PLAYER;
    ++gGeneration;
    std::lock_guard lock{gServerMutex};
    gServer = {};
}

// --- Join logging ---------------------------------------------------------------------------

int  gTicksSinceJoin   = -1; // client thread
bool gLoggedValues     = false;
int  gLastHunger       = -1;
int  gLastSat10        = -1;
int  gLastHealth10     = -1;
bool gLastExact        = false;
bool gLastPeaceful     = false;
bool gLastNaturalRegen = true;

// Logs the state once after joining and then whenever hunger, saturation, health (0.1 steps), source, peaceful or
// naturalRegen changes, so e2e scenarios can assert on it (tools/e2e: Wait-Hunger, Get-FoodStateLog).
void logValues(bool onlyOnChange) {
    auto s = readFoodState();
    if (!s) return;
    int sat10    = static_cast<int>(std::lround(s->saturation * 10));
    int health10 = static_cast<int>(std::lround(s->health * 10));
    if (onlyOnChange && s->hunger == gLastHunger && sat10 == gLastSat10 && health10 == gLastHealth10
        && s->exact == gLastExact && s->peaceful == gLastPeaceful && s->naturalRegen == gLastNaturalRegen)
        return;
    gLastHunger       = s->hunger;
    gLastSat10        = sat10;
    gLastHealth10     = health10;
    gLastExact        = s->exact;
    gLastPeaceful     = s->peaceful;
    gLastNaturalRegen = s->naturalRegen;
    log().info(
        "Food state: hunger={} saturation={:.2f} exhaustion={:.3f} health={:.1f}/{:.1f} peaceful={} naturalRegen={} "
        "creative={} source={}",
        s->hunger,
        s->saturation,
        s->exhaustion,
        s->health,
        s->maxHealth,
        s->peaceful,
        s->naturalRegen,
        s->creativeLike,
        s->exact ? "integrated-server" : "client"
    );
}

class PlayerFoodStateModule final : public app::Module {
public:
    [[nodiscard]] char const* name() const override { return "player-food-state"; }
    [[nodiscard]] int         order() const override { return -100; }

    void install() override {
        auto& bus   = ll::event::EventBus::getInstance();
        mServerTick = bus.emplaceListener<ll::event::ServerLevelTickEvent>([](ll::event::ServerLevelTickEvent& ev) {
            try {
                onServerTick(ev.level());
            } catch (...) {}
        });
        mJoin       = bus.emplaceListener<ll::event::ClientJoinLevelEvent>([](ll::event::ClientJoinLevelEvent&) {
            resetServerSnapshot();
            gTicksSinceJoin = 0;
            gLoggedValues   = false;
            log().info("BeetrootSkin: joined level");
        });
        mExit       = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](ll::event::ClientExitLevelEvent&) {
            resetServerSnapshot();
            gTicksSinceJoin = -1;
        });
        mClientTick = bus.emplaceListener<ll::event::ClientLevelTickEvent>([](ll::event::ClientLevelTickEvent&) {
            if (gTicksSinceJoin < 0) return; // not between join and exit
            auto* player = currentPlayer();
            gWantedId    = player ? player->getOrCreateUniqueID().rawID : NO_PLAYER;
            ++gTicksSinceJoin;
            // Give the server a moment to sync attributes before logging them once.
            if (!gLoggedValues && gTicksSinceJoin >= 40) {
                gLoggedValues = true;
                logValues(false);
            } else if (gLoggedValues) {
                logValues(true);
            }
        });
    }

    void uninstall() override {
        auto& bus = ll::event::EventBus::getInstance();
        for (auto* l : {&mServerTick, &mJoin, &mExit, &mClientTick}) {
            if (*l) bus.removeListener(*l);
            l->reset();
        }
        resetServerSnapshot();
    }

private:
    ll::event::ListenerPtr mServerTick, mJoin, mExit, mClientTick;
} gModule;
BS_REGISTER_MODULE(gModule);

} // namespace

namespace {
std::optional<core::FoodState> readFoodStateImpl(bool useServer) {
    auto* player = currentPlayer();
    auto* level  = currentLevel();
    if (!player || !level) return std::nullopt;

    core::FoodState s;
    s.hunger          = static_cast<int>(std::lround(attributeValue(*player, Player::HUNGER(), 20)));
    s.saturation      = attributeValue(*player, Player::SATURATION(), 0);
    s.exhaustion      = attributeValue(*player, Player::EXHAUSTION(), 0);
    s.health          = attributeValue(*player, SharedAttributes::HEALTH(), 20);
    s.maxHealth       = attributeMax(*player, SharedAttributes::HEALTH(), 20);
    s.peaceful        = level->getDifficulty() == SharedTypes::Legacy::Difficulty::Peaceful;
    s.hardcore        = level->isHardcore();
    s.naturalRegen    = naturalRegen(*level);
    s.creativeLike    = player->isCreative() || player->isSpectator();
    s.riding          = player->getVehicle() != nullptr;
    s.hasPoison       = player->getEffect(static_cast<unsigned>(core::effect_id::POISON)) != nullptr
                     || player->getEffect(static_cast<unsigned>(core::effect_id::FATAL_POISON)) != nullptr;
    s.hasWither       = player->getEffect(static_cast<unsigned>(core::effect_id::WITHER)) != nullptr;
    s.hasRegeneration = player->getEffect(static_cast<unsigned>(core::effect_id::REGENERATION)) != nullptr;

    auto server = useServer ? freshServerSnapshot(player->getOrCreateUniqueID().rawID) : std::nullopt;
    if (server) {
        s.hunger       = static_cast<int>(std::lround(server->hunger));
        s.saturation   = server->saturation;
        s.exhaustion   = server->exhaustion;
        s.health       = server->health;
        s.maxHealth    = server->maxHealth;
        s.peaceful     = server->peaceful;
        s.naturalRegen = server->naturalRegen;
        s.exact        = true;
    }
    return s;
}
} // namespace

std::optional<core::FoodState> readFoodState() {
    return readFoodStateImpl(!gForceClient.load(std::memory_order_relaxed));
}
std::optional<core::FoodState> readClientFoodState() { return readFoodStateImpl(false); }
std::optional<core::FoodState> readIntegratedServerFoodState() {
    auto s = readFoodStateImpl(true);
    if (!s || !s->exact) return std::nullopt;
    return s;
}

void setForceClientSource(bool force) { gForceClient.store(force, std::memory_order_relaxed); }
bool forceClientSource() { return gForceClient.load(std::memory_order_relaxed); }

std::optional<HeldFood> heldFood() {
    auto* player = currentPlayer();
    if (!player) return std::nullopt;
    if (auto food = getFoodValues(player->getSelectedItem(), player)) return HeldFood{std::move(*food), false};
    if (app::config().showFoodValuesHudOverlayWhenOffhand) {
        if (auto food = getFoodValues(player->getOffhandSlot(), player)) return HeldFood{std::move(*food), true};
    }
    return std::nullopt;
}

} // namespace beetroot::game

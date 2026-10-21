// AppleSkin's food debug info: the F3 line "hunger: N, sat: #.##, exh: 0.00/4".
//
// Bedrock has no F3 screen, so the line is drawn on the HUD (top left, above the vanilla
// "Position" / "Days played" text and chat, see the layout notes below) while
// Config::showFoodDebugInfo is true. A remappable hotkey (default F6, listed in Settings >
// Keyboard & Mouse as "Toggle food debug info") toggles the option, saves the config and
// shows a short action-bar message.
//
// Hotkey plumbing follows BEI (src/input/Hotkeys.cpp): the key is created with LeviLamina's
// KeyRegistry during BeetrootSkin::load() (Module::onLoad; the game reads registered keys into
// its keyboard layout before mods are enabled), bound in no input mapping, and detected by
// matching the raw KeyInputEvent stream (window thread) against the key codes currently
// mapped in the game's keyboard layout. The toggle itself runs on the client thread. The key's
// name in the controls menu comes from app/KeyNames.cpp ("beetroot.key.<name>").
#include "core/food/DebugInfo.h"
#include "app/I18n.h"
#include "app/Mod.h"
#include "app/Module.h"
#include "game/Game.h"
#include "game/PlayerFoodState.h"
#include "mod/Log.h"
#include "ui/gfx/Painter.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/input/KeyInputEvent.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "ll/api/event/world/ClientLevelTickEvent.h"
#include "ll/api/input/KeyRegistry.h"
#include "ll/api/memory/Hook.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/GuiData.h"
#include "mc/client/gui/screens/UIScene.h"
#include "mc/client/input/KeyboardRemappingLayout.h"
#include "mc/client/input/Keymapping.h"
#include "mc/client/input/RemappingLayout.h"
#include "mc/client/input/VanillaClientInputMappingFactory.h"
#include "mc/client/multiplayer/ClientLevel.h"
#include "mc/client/options/IOptionRegistry.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"

#include <array>
#include <atomic>
#include <chrono>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace beetroot::features::debug_info {

namespace {

constexpr std::string_view KEY_NAME      = "toggle_food_debug_info"; ///< KeyRegistry name.
constexpr int              VK_F6         = 0x75;
constexpr std::string_view MOD_PREFIX    = "key.BeetrootSkin."; ///< Keymapping actions: "key.<mod>.<name>".
constexpr auto             REPEAT_WINDOW = std::chrono::milliseconds(1100);

// --- Layout (GUI units) -----------------------------------------------------------------------
// Verified in-game (1600x900, GUI scale 3): vanilla's top-left stack ("Position: x, y, z",
// "Days played: N", then chat messages) starts about 50 units down, below the band reserved
// for the paper doll (off by default). Our one line fits in that band at the very top, like
// Java's F3 text, so it never overlaps the position/day text or chat.
constexpr float MARGIN_X = 2; ///< Left edge of our background box.
constexpr float TOP_Y    = 2; ///< Top of our background box.
constexpr float PAD      = 1; ///< Our box's padding around the text.

/// Key codes currently mapped to the hotkey (0 = none); F6 until the layout was read. Written
/// on the client thread (layout rebuild), read on the window thread.
std::array<std::atomic<int>, 2> gKeys{VK_F6, 0};
/// readLayout() ran at least once (window/client threads never race on this: client only).
bool gLayoutRead = false;
/// The HUD is the top screen of a level (no menu, chat or inventory): hotkey allowed.
std::atomic<bool> gHudTop{false};
/// Toggle requests from the window thread, consumed on the client thread.
std::atomic<int> gPendingToggles{0};
/// Physical key state and time of the last "down", to ignore OS key repeat (window thread only).
std::array<bool, 256>                                  gKeyDown{};
std::array<std::chrono::steady_clock::time_point, 256> gKeyDownAt{};

std::string actionName() { return std::string{MOD_PREFIX} + std::string{KEY_NAME}; }

void readLayout(RemappingLayout const& layout) {
    auto const  action  = actionName();
    auto const& mapping = layout.getKeymappingByAction(action);
    if (*mapping.mAction != action) { // not in the layout: keep the current keys
        static bool warned = false;
        if (!warned) log().warn("Food debug info hotkey {} is not in the keyboard layout", action);
        warned = true;
        return;
    }
    std::array<int, 2> keys{};
    std::size_t        n = 0;
    if (mapping.isAssigned() && !mapping.isAltKey()) { // mouse buttons are "alt keys"
        for (int key : *mapping.mKeys) {
            if (key > 0 && key < 256 && n < keys.size()) keys[n++] = key;
        }
    }
    if (!gLayoutRead || keys[0] != gKeys[0].load() || keys[1] != gKeys[1].load()) {
        log().info("Food debug info hotkey mapped to key code {} {}", keys[0], keys[1]);
    }
    gLayoutRead = true;
    for (std::size_t i = 0; i < keys.size(); ++i) gKeys[i] = keys[i];
}

void toggle() {
    auto& cfg             = app::config();
    cfg.showFoodDebugInfo = !cfg.showFoodDebugInfo;
    app::saveConfig();
    bool on = cfg.showFoodDebugInfo;
    log().info("Food debug info {}", on ? "shown" : "hidden");
    auto* client = game::currentClient();
    if (!client) return;
    auto message = on ? app::tr("beetroot.debug_info.toggled_on", "AppleSkin debug info: on")
                      : app::tr("beetroot.debug_info.toggled_off", "AppleSkin debug info: off");
    client->getGuiData()->setActionBarMessage(message, std::nullopt);
}

/// Client thread: refresh gHudTop. Called every UI frame (level ticks stop while a pausing
/// screen is open, so the tick alone would leave a stale "true") and on every client tick.
bool updateHudTop() {
    auto* client = game::currentClient();
    bool  hudTop = client && game::currentLevel() && client->getTopScreenName().starts_with("hud_screen");
    gHudTop      = hudTop;
    return hudTop;
}

void onClientTick() {
    bool hudTop  = updateHudTop();
    int  pending = gPendingToggles.exchange(0);
    if (!hudTop) return; // a screen opened since the key press
    for (int i = 0; i < pending; ++i) toggle();
}

void render(MinecraftUIRenderContext& ctx) {
    if (!app::config().showFoodDebugInfo) return;
    if (!ctx.mCurrentScene.getScreenName().starts_with("hud_screen")) return;
    auto* client = game::currentClient();
    auto* level  = game::currentLevel();
    if (!client || !level || client->getOptions().getHideHud()) return;
    auto state = game::readFoodState();
    if (!state) return;

    ui::Painter painter(ctx);
    auto const  text = core::foodDebugLine(*state);
    // Values not read from the integrated server: the client's synced copy (remote servers).
    auto const  suffix = state->exact ? std::string{} : " " + app::tr("beetroot.debug_info.source_client", "(client)");
    float const top    = TOP_Y;
    float const textW  = painter.textWidth(text);
    float const suffW  = suffix.empty() ? 0.f : painter.textWidth(suffix);
    float const lineH  = painter.lineHeight();
    // Java's F3 background (0x90505050) behind the text keeps it readable over chat and terrain.
    painter.fillRect({MARGIN_X, top, textW + suffW + 2 * PAD, lineH + 2 * PAD}, ui::Color::rgb(0x505050, 0x90 / 255.f));
    painter.text(text, MARGIN_X + PAD, top + PAD, ui::palette::TEXT_LIGHT);
    if (!suffix.empty()) painter.text(suffix, MARGIN_X + PAD + textW, top + PAD, ui::palette::TEXT_DIM);
    painter.flush();
}

} // namespace

// --- Hooks ------------------------------------------------------------------------------------

// Runs whenever the keyboard controls are (re)built, including after the player remaps a key.
LL_TYPE_INSTANCE_HOOK(
    DebugInfoKeyLayoutHook,
    ll::memory::HookPriority::Normal,
    VanillaClientInputMappingFactory,
    &VanillaClientInputMappingFactory::$_updateKeyboardAndMouseControls,
    void,
    IOptionRegistry& options
) {
    origin(options);
    try {
        if (auto layout = mKeyboardRemappingLayout.lock()) readLayout(*layout);
    } catch (...) {}
}

namespace {

bool gLayoutHooked = false; // main thread (load/enable/disable)

void hookLayout() {
    if (gLayoutHooked) return;
    DebugInfoKeyLayoutHook::hook();
    gLayoutHooked = true;
}

void unhookLayout() {
    if (!gLayoutHooked) return;
    DebugInfoKeyLayoutHook::unhook();
    gLayoutHooked = false;
}

/// Window thread: a raw key event. Only atomics and window-thread state here.
void onKey(ll::event::KeyInputEvent const& ev) {
    int vk = ev.keyCode();
    if (vk <= 0 || vk >= static_cast<int>(gKeyDown.size())) return;
    auto const i    = static_cast<std::size_t>(vk);
    bool const down = ev.isDown();
    if (!down) {
        gKeyDown[i] = false;
        return;
    }
    // OS key repeat re-sends "down" every ~30 ms after a delay of at most ~1 s. A "down" long
    // after the last one is a new press even if its "up" was lost (e.g. released while the
    // window had no focus).
    auto const now    = std::chrono::steady_clock::now();
    bool const repeat = gKeyDown[i] && now - gKeyDownAt[i] < REPEAT_WINDOW;
    gKeyDown[i]       = true;
    gKeyDownAt[i]     = now;
    if (repeat || !gHudTop.load()) return;
    if (vk == gKeys[0].load() || vk == gKeys[1].load()) gPendingToggles.fetch_add(1);
}

class DebugInfoModule final : public app::Module {
public:
    [[nodiscard]] char const* name() const override { return "debug-info"; }

    void onLoad() override {
        // The layout is built (and our key read from it) before enable(): hook it now.
        hookLayout();
        auto& key = ll::input::KeyRegistry::getInstance().getOrCreateKey(KEY_NAME, {VK_F6}, true);
        key.setInputMappingStack({}); // listed and remappable, but bound in no input mapping
        log().debug("Registered key {}", key.getFullName());
    }

    void install() override {
        hookLayout(); // again after a disable/enable cycle (keys stay registered)
        auto& bus = ll::event::EventBus::getInstance();
        mKey      = bus.emplaceListener<ll::event::KeyInputEvent>([](ll::event::KeyInputEvent& ev) { onKey(ev); });
        mTick     = bus.emplaceListener<ll::event::ClientLevelTickEvent>([](ll::event::ClientLevelTickEvent&) {
            try {
                onClientTick();
            } catch (std::exception const& e) {
                log().error("Food debug info toggle failed: {}", e.what());
            }
        });
        mExit     = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](ll::event::ClientExitLevelEvent&) {
            gHudTop         = false;
            gPendingToggles = 0;
        });
        mRender   = bus.emplaceListener<ll::event::AfterUIRenderEvent>([](ll::event::AfterUIRenderEvent& ev) {
            try {
                updateHudTop();
                render(ev.uiRenderContext());
            } catch (std::exception const& e) {
                static bool logged = false;
                if (!logged) log().error("Food debug info render failed: {}", e.what());
                logged = true;
            }
        });
    }

    void uninstall() override {
        auto& bus = ll::event::EventBus::getInstance();
        for (auto* l : {&mKey, &mTick, &mExit, &mRender}) {
            if (*l) bus.removeListener(*l);
            l->reset();
        }
        unhookLayout();
        gHudTop         = false;
        gPendingToggles = 0;
    }

private:
    ll::event::ListenerPtr mKey, mTick, mExit, mRender;
} gModule;
BS_REGISTER_MODULE(gModule);

} // namespace

} // namespace beetroot::features::debug_info

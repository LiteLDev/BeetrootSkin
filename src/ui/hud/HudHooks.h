#pragma once

// Contract for drawing on the vanilla HUD around the hunger bar and the hearts.
//
// ui/hud/HudHooks.cpp hooks HudHungerRenderer::render and HudHeartRenderer::render (HUD screen
// only). For each call it builds a HudContext and runs the registered layers of the matching
// slots: Below* before the vanilla icons are drawn, Above* after them.
//
//     class MyLayer final : public beetroot::ui::IHudLayer {
//     public:
//         HudSlot slot() const override { return HudSlot::AboveHunger; }
//         void    render(HudContext& ctx) override { ... ctx.painter ... ctx.hungerIcons[i] ... }
//     } gMyLayer;
//     BS_REGISTER_HUD_LAYER(gMyLayer);

#include "core/food/FoodMath.h"
#include "core/geom/Rect.h"
#include "game/PlayerFoodState.h"

#include <array>
#include <optional>
#include <vector>

namespace beetroot::ui {

class Painter;

enum class HudSlot {
    BelowHunger, ///< Before the vanilla hunger icons (e.g. exhaustion underlay).
    AboveHunger, ///< After the vanilla hunger icons (saturation / food value overlays).
    BelowHearts, ///< Before the vanilla heart icons.
    AboveHearts, ///< After the vanilla heart icons (health gain overlay).
};

/// Per-call information for HUD layers. GUI units throughout. Only valid during render().
struct HudContext {
    Painter& painter; ///< Draws immediately around the vanilla bar; flushed after each layer.
    HudSlot  slot;

    /// The UIControl that owns the renderer being drawn (its cached position and size).
    core::Rect control;

    /// Hunger icons (9x9 each). Index 0 is the RIGHTMOST shank (hunger points 1-2), index 9 the
    /// leftmost (19-20), like AppleSkin's icon index i <-> hunger 2i+1..2i+2. Includes the
    /// vanilla per-icon jitter when the game applied any. `hungerIconsValid` is false until the
    /// hunger renderer has been seen once (then the rects are a best-effort layout).
    std::array<core::Rect, core::ICONS_PER_ROW> hungerIcons{};
    bool                                        hungerIconsValid = false;
    /// Vanilla jitter of each hunger icon relative to its resting position (y only in vanilla;
    /// resting line learned from still frames, see core::RestingRows). Subtract it from the rect
    /// to draw at rest (showVanillaAnimationsOverlay off).
    std::array<float, core::ICONS_PER_ROW> hungerJitterY{};

    /// Heart icons (9x9 each) in vanilla order: index 0 is the leftmost heart of the bottom row
    /// (health 1-2), continuing rightwards and then upwards for further rows. Absorption hearts
    /// are not included. Empty until the heart renderer has been seen once.
    std::vector<core::Rect> heartIcons;
    /// Vanilla jitter (low-health shake) of each heart relative to its resting row, like
    /// hungerJitterY; same size as heartIcons.
    std::vector<float> heartJitterY;

    /// Local player state (nullopt when unavailable) and the food in hand (main hand first,
    /// offhand if allowed by config). Both read once per frame.
    std::optional<core::FoodState> state;
    std::optional<game::HeldFood>  held;

    /// AppleSkin flash alpha for the "food value" overlays: FlashAnimator alpha already scaled
    /// by Config::maxHudOverlayFlashAlpha (0 when nothing is held).
    float flashAlpha = 0;
    /// Client ticks since the mod was enabled (20 Hz while a level ticks).
    int clientTick = 0;
};

/// A HUD drawing unit. Registered once at static-init time; lives for the whole session.
/// render() runs on the client thread inside the vanilla HUD render pass.
class IHudLayer {
public:
    virtual ~IHudLayer() = default;

    [[nodiscard]] virtual HudSlot slot() const = 0;
    /// Layers of the same slot run in ascending order().
    [[nodiscard]] virtual int order() const { return 0; }
    virtual void              render(HudContext& ctx) = 0;
};

/// All registered layers (registration order). Static-init safe.
[[nodiscard]] std::vector<IHudLayer*>& hudLayers();

/// Client ticks counted by the HUD module (ClientLevelTickEvent, 20 Hz while in a level).
[[nodiscard]] int clientTickCount();
/// Current AppleSkin flash alpha (FlashAnimator x Config::maxHudOverlayFlashAlpha); advanced
/// once per client tick while food is held, reset otherwise.
[[nodiscard]] float hudFlashAlpha();

struct HudLayerRegistrar {
    explicit HudLayerRegistrar(IHudLayer& layer) { hudLayers().push_back(&layer); }
};

} // namespace beetroot::ui

#define BS_HUD_DETAIL_CONCAT2(a, b) a##b
#define BS_HUD_DETAIL_CONCAT(a, b)  BS_HUD_DETAIL_CONCAT2(a, b)
/// Register an IHudLayer instance (an object with static storage duration).
#define BS_REGISTER_HUD_LAYER(obj)                                                                                     \
    [[maybe_unused]] static ::beetroot::ui::HudLayerRegistrar BS_HUD_DETAIL_CONCAT(bsHudLayerRegistrar_, __LINE__) {   \
        obj                                                                                                            \
    }

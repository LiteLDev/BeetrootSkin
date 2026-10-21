#pragma once

#include "core/geom/Rect.h"

#include <cstdint>
#include <string>
#include <vector>

class MinecraftUIRenderContext;
class ItemStack;

namespace beetroot::ui {

/// RGBA color, components in [0,1].
struct Color {
    float r = 1, g = 1, b = 1, a = 1;

    static constexpr Color rgb(std::uint32_t hex, float alpha = 1.f) {
        return {((hex >> 16) & 0xFF) / 255.f, ((hex >> 8) & 0xFF) / 255.f, (hex & 0xFF) / 255.f, alpha};
    }
    [[nodiscard]] constexpr Color withAlpha(float alpha) const { return {r, g, b, alpha}; }

    friend bool operator==(Color const&, Color const&) = default;
};

namespace palette {
inline constexpr Color WHITE        = Color::rgb(0xFFFFFF);
inline constexpr Color TEXT         = Color::rgb(0x404040);
inline constexpr Color TEXT_LIGHT   = Color::rgb(0xFFFFFF);
inline constexpr Color TEXT_DIM     = Color::rgb(0xA0A0A0);
inline constexpr Color TOOLTIP_BG   = Color::rgb(0x100010, 0.94f);
inline constexpr Color TOOLTIP_EDGE = Color::rgb(0x5000FF, 0.6f);
} // namespace palette

/// A texture to draw: either a resource-pack path ("textures/ui/arrow", no extension), or a
/// mod-owned file under bin/BeetrootSkin (assets/textures/** is copied there at build time),
/// e.g. TextureRef::mod("textures/icons.png"). Mod-owned files are loaded by absolute path
/// through the game's raw file system (ResourceFileSystem::Raw).
struct TextureRef {
    std::string path;
    bool        modOwned = false;

    [[nodiscard]] static TextureRef pack(std::string p) { return {std::move(p), false}; }
    [[nodiscard]] static TextureRef mod(std::string relative) { return {std::move(relative), true}; }

    friend bool operator==(TextureRef const&, TextureRef const&) = default;
};

/// AppleSkin's icon sheet (256x256, see core::sprites) and tooltip outline (9x9).
namespace textures {
inline TextureRef const ICONS{"textures/icons.png", true};
inline TextureRef const TOOLTIP_HUNGER_OUTLINE{"textures/tooltip_hunger_outline.png", true};
} // namespace textures

enum class TextAlign { Left, Center, Right };

/// Thin batching wrapper over MinecraftUIRenderContext.
///
/// The game's UI renderer needs a strict order: rectangles -> flushImages, item icons,
/// text -> flushText. Callers may record in any order; flush() replays commands layer by
/// layer (Background, Items, Foreground, Text). Call flush() once per "z band"; anything
/// drawn after a flush() appears above everything recorded before it.
///
/// All coordinates are GUI units (the same space as the vanilla UI's controls).
/// Only valid inside a UI render callback; never keep a Painter across frames.
class Painter {
public:
    enum class Layer { Background, Items, Foreground, Text };

    explicit Painter(MinecraftUIRenderContext& ctx);
    ~Painter(); // flushes
    Painter(Painter const&)            = delete;
    Painter& operator=(Painter const&) = delete;

    void fillRect(core::Rect const& r, Color c, Layer layer = Layer::Background);
    void outlineRect(core::Rect const& r, Color c, float thickness = 1, Layer layer = Layer::Background);

    /// Draw an item icon (16x16 * scale) of a stack the caller owns (valid until flush()).
    void itemStack(ItemStack const& stack, float x, float y, float scale = 1, float alpha = 1);

    void text(
        std::string const& s,
        float              x,
        float              y,
        Color              c     = palette::TEXT_LIGHT,
        float              scale = 1,
        TextAlign          align = TextAlign::Left,
        float              width = 0
    );
    [[nodiscard]] float textWidth(std::string const& s, float scale = 1) const;
    [[nodiscard]] float lineHeight(float scale = 1) const { return 9 * scale; }

    /// Whole texture stretched into `dst`.
    void texture(std::string const& path, core::Rect const& dst, float alpha = 1);
    void texture(TextureRef const& tex, core::Rect const& dst, float alpha = 1, Layer layer = Layer::Foreground);
    /// Sub-rectangle `src` (u, v, w, h in texture pixels) of a `texW` x `texH` texture drawn into
    /// `dst`, multiplied by `tint` (tint.a is the opacity). E.g. AppleSkin's saturation icon:
    /// texture(textures::ICONS, {x, y, 9, 9}, {27, 0, 9, 9}, 256, 256).
    void texture(
        TextureRef const& tex,
        core::Rect const& dst,
        core::Rect const& src,
        float             texW,
        float             texH,
        Color             tint  = palette::WHITE,
        Layer             layer = Layer::Foreground
    );
    /// Nine-slice a `texW`x`texH` texture with a `slice`-pixel border into `r`.
    void nineSlice(
        TextureRef const& tex,
        core::Rect const& r,
        float             texW,
        float             texH,
        float             slice,
        Layer             layer = Layer::Background
    );

    /// True once the texture is loaded and drawable (mod textures may load asynchronously;
    /// commands for a texture that is not ready yet draw nothing).
    [[nodiscard]] bool textureReady(TextureRef const& tex) const;

    /// Replay all recorded commands in layer order.
    void flush();

    /// Screen size in GUI units.
    [[nodiscard]] core::Rect                screen() const;
    [[nodiscard]] MinecraftUIRenderContext& raw() const { return mCtx; }

private:
    struct Cmd;
    MinecraftUIRenderContext& mCtx;
    std::vector<Cmd>*         mCmds; // pimpl-ish to keep Cmd out of the header
};

} // namespace beetroot::ui

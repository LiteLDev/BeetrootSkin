#include "ui/gfx/Painter.h"

#include "app/Mod.h"
#include "mod/Log.h"

#include "mc/client/game/IClientInstance.h"
#include "mc/client/game/IMinecraftGame.h"
#include "mc/client/gui/CaretMeasureData.h"
#include "mc/client/gui/Font.h"
#include "mc/client/gui/FontHandle.h"
#include "mc/client/gui/GuiData.h"
#include "mc/client/gui/TextAlignment.h"
#include "mc/client/gui/TextMeasureData.h"
#include "mc/client/gui/controls/UIMaterialType.h"
#include "mc/client/gui/controls/renderers/InventoryItemRenderer.h"
#include "mc/client/gui/screens/BatchClippingState.h"
#include "mc/client/gui/screens/BatchKey.h"
#include "mc/client/gui/screens/ComponentRenderBatch.h"
#include "mc/client/gui/screens/UIBatchType.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/renderer/actor/ItemRenderChunkType.h"
#include "mc/client/renderer/actor/ItemRenderer.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/deps/core/file/PathView.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/core/resource/ResourceFileSystem.h"
#include "mc/deps/core/resource/ResourceLocation.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/deps/input/RectangleArea.h"
#include "mc/deps/minecraft_renderer/renderer/BedrockTextureData.h"
#include "mc/deps/minecraft_renderer/renderer/TextureLoadState.h"
#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStack.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <memory>
#include <new>
#include <unordered_map>
#include <variant>

namespace beetroot::ui {

namespace {
struct RectCmd {
    core::Rect r;
    Color      c;
};
struct ItemCmd {
    ItemStack const* stack; // caller-owned, valid within the frame
    float            x, y, scale, alpha;
};
struct TextCmd {
    std::string s;
    float       x, y;
    Color       c;
    float       scale;
    TextAlign   align;
    float       width;
};
struct TextureCmd {
    TextureRef tex;
    core::Rect r;
    float      u0 = 0, v0 = 0, u1 = 1, v1 = 1; // normalized UV rectangle
    Color      tint;
};

mce::Color toMce(Color c) { return mce::Color{c.r, c.g, c.b, c.a}; }

RectangleArea toArea(core::Rect const& r) { return RectangleArea{r.x, r.right(), r.y, r.bottom()}; }

Font& uiFont(MinecraftUIRenderContext& ctx) { return ctx.mClient.getFontHandle().getFont(); }

HashedString const& fillMaterial() {
    static HashedString const m{"ui_fillColor"};
    return m;
}
// Material the vanilla UI uses to flush textured quads.
HashedString const& imageMaterial() {
    static HashedString const m{"ui_flush"};
    return m;
}

/// Absolute, forward-slash path of a mod-owned texture.
std::string modTexturePath(std::string const& relative) {
    auto p = app::modFile(relative).generic_string();
    return p;
}

ResourceLocation locationOf(TextureRef const& tex) {
    if (tex.modOwned) return ResourceLocation(Core::PathView(modTexturePath(tex.path)), ResourceFileSystem::Raw);
    return ResourceLocation(Core::PathView(tex.path));
}

std::string cacheKey(TextureRef const& tex) { return (tex.modOwned ? "mod:" : "pack:") + tex.path; }

bool isDrawable(BedrockTextureData const* data) {
    if (!data) return false;
    if (data->mIsMissingTexture == IsMissingTexture::Yes) return false;
    return (static_cast<unsigned>(data->mTextureLoadState) & static_cast<unsigned>(TextureLoadState::LoadedBit)) != 0;
}

/// Log a mod texture's load state once per state change (helps diagnosing missing art).
void traceModTexture(TextureRef const& tex, BedrockTextureData const* data) {
    static std::unordered_map<std::string, int> seen;
    int state           = !data ? -1 : static_cast<int>(data->mTextureLoadState) | (isDrawable(data) ? 0x100 : 0);
    auto [it, inserted] = seen.try_emplace(tex.path, state);
    if (!inserted && it->second == state) return;
    it->second = state;
    log().debug(
        "Mod texture {}: {}",
        modTexturePath(tex.path),
        !data              ? "no data"
        : isDrawable(data) ? "loaded"
                           : "pending"
    );
}

// Render chunk types the vanilla slot renderer batches (InventoryItemRenderer::getRenderTypeFromItem):
// block models on the terrain atlas and flat sprites on the item atlas. Anything else (and any
// glinting stack) goes through ItemRenderer::renderGuiItemNew directly.
constexpr int CHUNK_BLOCK = 0;
constexpr int CHUNK_FLAT  = 2;

/// A ComponentRenderBatch set up like the one a vanilla inventory slot uses for its icons
/// (shared mesh, "Item" material, atlas texture). The type has no usable constructor, so it is
/// built in place in zeroed storage and destroyed with its own destructor.
class ItemMeshBatch {
public:
    ItemMeshBatch(int chunk, int depth, float alpha) {
        std::memset(mStorage, 0, sizeof(mStorage));
        auto&                                 batch = get();
        alignas(BatchClippingState) std::byte clip[sizeof(BatchClippingState)]{}; // no clipping
        auto*                                 key = ::new (std::addressof(batch.mBatchKey.get()))
            BatchKey(depth, alpha, *std::launder(reinterpret_cast<BatchClippingState*>(clip)));
        key->mBatchType      = UIBatchType::SharedMesh;
        key->mUIMaterialType = UIMaterialType::Item;
        auto& textures       = key->mResourceLocations.get();
        setLocation(textures[0], chunk == CHUNK_BLOCK ? "atlas.terrain" : "atlas.items");
        batch.mIsDirty                = true;
        batch.mRequiresPreRenderSetup = false;
        batch.mRenderPass             = 0;
        ::new (std::addressof(batch.mCustomRenderInstances.get())) std::vector<CustomRenderComponent*>();
        ::new (std::addressof(batch.mSpriteInstances.get())) std::vector<SpriteComponent*>();
        ::new (std::addressof(batch.mTextInstances.get())) std::vector<TextComponent*>();
    }
    ~ItemMeshBatch() { std::destroy_at(&get()); }
    ItemMeshBatch(ItemMeshBatch const&)            = delete;
    ItemMeshBatch& operator=(ItemMeshBatch const&) = delete;

    ComponentRenderBatch& get() { return *std::launder(reinterpret_cast<ComponentRenderBatch*>(mStorage)); }

private:
    static void setLocation(ResourceLocation& loc, char const* path) {
        std::destroy_at(&loc);
        ::new (&loc) ResourceLocation(Core::PathView(path));
    }
    alignas(ComponentRenderBatch) std::byte mStorage[sizeof(ComponentRenderBatch)];
};

/// Durability of a stack as remaining/max in [0,1], or a negative value if it has none.
float durabilityRatio(ItemStack const& stack) {
    if (!stack.mItem || !stack.isDamageableItem() || !stack.isDamaged()) return -1;
    short maxDamage = stack.mItem->getMaxDamage();
    if (maxDamage <= 0) return -1;
    float remaining = static_cast<float>(maxDamage - stack.getDamageValue());
    return std::clamp(remaining / static_cast<float>(maxDamage), 0.f, 1.f);
}
} // namespace

struct Painter::Cmd {
    Layer                                               layer;
    std::variant<RectCmd, ItemCmd, TextCmd, TextureCmd> v;
};

Painter::Painter(MinecraftUIRenderContext& ctx) : mCtx(ctx), mCmds(new std::vector<Cmd>) {}

Painter::~Painter() {
    try {
        flush();
    } catch (...) {}
    delete mCmds;
}

void Painter::fillRect(core::Rect const& r, Color c, Layer layer) {
    if (r.empty()) return;
    mCmds->push_back({
        layer,
        RectCmd{r, c}
    });
}

void Painter::outlineRect(core::Rect const& r, Color c, float t, Layer layer) {
    fillRect({r.x, r.y, r.w, t}, c, layer);
    fillRect({r.x, r.bottom() - t, r.w, t}, c, layer);
    fillRect({r.x, r.y + t, t, r.h - 2 * t}, c, layer);
    fillRect({r.right() - t, r.y + t, t, r.h - 2 * t}, c, layer);
}

void Painter::nineSlice(TextureRef const& tex, core::Rect const& r, float texW, float texH, float slice, Layer layer) {
    if (r.empty() || texW <= 0 || texH <= 0) return;
    float s = std::min({slice, r.w / 2, r.h / 2});
    float xs[4]{r.x, r.x + s, r.right() - s, r.right()};
    float ys[4]{r.y, r.y + s, r.bottom() - s, r.bottom()};
    float us[4]{0, slice / texW, 1 - slice / texW, 1};
    float vs[4]{0, slice / texH, 1 - slice / texH, 1};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            core::Rect part{xs[col], ys[row], xs[col + 1] - xs[col], ys[row + 1] - ys[row]};
            if (part.empty()) continue;
            mCmds->push_back({
                layer,
                TextureCmd{tex, part, us[col], vs[row], us[col + 1], vs[row + 1], palette::WHITE}
            });
        }
    }
}

void Painter::itemStack(ItemStack const& stack, float x, float y, float scale, float alpha) {
    mCmds->push_back({
        Layer::Items,
        ItemCmd{&stack, x, y, scale, alpha}
    });
}

void Painter::text(std::string const& s, float x, float y, Color c, float scale, TextAlign align, float width) {
    if (s.empty()) return;
    mCmds->push_back({
        Layer::Text,
        TextCmd{s, x, y, c, scale, align, width}
    });
}

float Painter::textWidth(std::string const& s, float scale) const {
    return static_cast<float>(mCtx.getLineLength(uiFont(mCtx), s, scale, false));
}

void Painter::texture(std::string const& path, core::Rect const& r, float alpha) {
    texture(TextureRef::pack(path), r, alpha);
}

void Painter::texture(TextureRef const& tex, core::Rect const& r, float alpha, Layer layer) {
    if (r.empty()) return;
    mCmds->push_back({
        layer,
        TextureCmd{tex, r, 0, 0, 1, 1, palette::WHITE.withAlpha(alpha)}
    });
}

void Painter::texture(
    TextureRef const& tex,
    core::Rect const& dst,
    core::Rect const& src,
    float             texW,
    float             texH,
    Color             tint,
    Layer             layer
) {
    if (dst.empty() || src.empty() || !(texW > 0) || !(texH > 0)) return;
    mCmds->push_back({
        layer,
        TextureCmd{tex, dst, src.x / texW, src.y / texH, src.right() / texW, src.bottom() / texH, tint}
    });
}

bool Painter::textureReady(TextureRef const& tex) const {
    auto ptr = mCtx.getTexture(locationOf(tex), false);
    return isDrawable(ptr.mClientTexture.get());
}

core::Rect Painter::screen() const {
    auto gui = mCtx.mClient.getGuiData();
    auto sz  = gui->mScreenSizeData->clientUIScreenSize;
    return {0, 0, sz->x, sz->y};
}

void Painter::flush() {
    if (mCmds->empty()) return;
    mce::Color const white{1.f, 1.f, 1.f, 1.f};

    // Textures are resolved once per flush; a TexturePtr is never kept across frames.
    std::unordered_map<std::string, mce::TexturePtr> textures;
    auto clientTexture = [&](TextureRef const& tex) -> BedrockTextureData const* {
        auto key = cacheKey(tex);
        auto it  = textures.find(key);
        if (it == textures.end()) it = textures.emplace(key, mCtx.getTexture(locationOf(tex), false)).first;
        auto const* data = it->second.mClientTexture.get();
        if (tex.modOwned) traceModTexture(tex, data);
        return isDrawable(data) ? data : nullptr;
    };

    // Rectangles and textured quads of one layer, in submission order (later commands draw on
    // top). Consecutive commands of the same kind and tint share one flushImages call.
    auto quads = [&](Layer layer) {
        enum class Run { None, Fill, Image };
        Run   run     = Run::None;
        Color runTint = palette::WHITE;
        auto  endRun  = [&] {
            if (run == Run::Fill) mCtx.flushImages(white, 1.f, fillMaterial());
            if (run == Run::Image) mCtx.flushImages(toMce(runTint.withAlpha(1.f)), runTint.a, imageMaterial());
            run = Run::None;
        };
        for (auto& cmd : *mCmds) {
            if (cmd.layer != layer) continue;
            if (auto* rc = std::get_if<RectCmd>(&cmd.v)) {
                if (run != Run::Fill) endRun(), run = Run::Fill;
                mCtx.fillRectangle(toArea(rc->r), toMce(rc->c), rc->c.a);
                continue;
            }
            auto* tc = std::get_if<TextureCmd>(&cmd.v);
            if (!tc) continue;
            auto const* data = clientTexture(tc->tex);
            if (!data) continue;
            if (run != Run::Image || runTint != tc->tint) endRun(), run = Run::Image, runTint = tc->tint;
            mCtx.drawImage(
                data->mClientTexture,
                {tc->r.x, tc->r.y},
                {tc->r.w, tc->r.h},
                {tc->u0, tc->v0},
                {tc->u1 - tc->u0, tc->v1 - tc->v0},
                false
            );
        }
        endRun();
    };

    quads(Layer::Background);

    // Items. Flush an (off-screen) quad first so no half-built image batch is pending when the
    // item renderer starts emitting meshes, then draw icons the way vanilla slots do.
    std::vector<ItemCmd const*> items;
    for (auto& cmd : *mCmds) {
        auto* ic = std::get_if<ItemCmd>(&cmd.v);
        if (!ic || !ic->stack || ic->stack->isNull() || !ic->stack->mItem) continue;
        items.push_back(ic);
    }
    auto* renderer = mCtx.mClient.getItemRenderer();
    if (renderer && !items.empty()) {
        mCtx.fillRectangle(RectangleArea{-10000.f, -9999.f, -10000.f, -9999.f}, white, 1.f);
        mCtx.flushImages(white, 1.f, imageMaterial());

        BaseActorRenderContext rc(mCtx.mScreenContext, mCtx.mClient, mCtx.mClient.getMinecraftGame_DEPRECATED());
        auto                   direct = [&](ItemCmd const& c, bool foil) {
            renderer->renderGuiItemNew(rc, *c.stack, 0, c.x, c.y, foil, c.alpha, 1.f, c.scale, 0);
        };
        std::vector<int> chunks(items.size());
        for (std::size_t i = 0; i < items.size(); ++i) {
            auto const& stack = *items[i]->stack;
            chunks[i]         = stack.mItem->isGlint(stack)
                                  ? -1
                                  : static_cast<int>(InventoryItemRenderer::getRenderTypeFromItem(stack));
        }
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (chunks[i] != -1) continue;
            direct(*items[i], false);
            direct(*items[i], true);
        }
        std::vector<float> alphas;
        for (auto const* it : items) {
            if (std::find(alphas.begin(), alphas.end(), it->alpha) == alphas.end()) alphas.push_back(it->alpha);
        }
        for (int chunk : {CHUNK_BLOCK, CHUNK_FLAT}) {
            for (float alpha : alphas) {
                bool any = false;
                for (std::size_t i = 0; i < items.size() && !any; ++i)
                    any = chunks[i] == chunk && items[i]->alpha == alpha;
                if (!any) continue;
                ItemMeshBatch batch(chunk, 0, alpha);
                mCtx.beginSharedMeshBatch(batch.get());
                for (std::size_t i = 0; i < items.size(); ++i) {
                    if (chunks[i] != chunk || items[i]->alpha != alpha) continue;
                    auto const& c = *items[i];
                    renderer->renderGuiItemInChunk(
                        rc,
                        static_cast<ItemRenderChunkType>(chunk),
                        *c.stack,
                        c.x,
                        c.y,
                        1.f,
                        chunk == CHUNK_BLOCK ? 0.f : 1.f,
                        c.scale,
                        0,
                        false,
                        0,
                        std::nullopt
                    );
                }
                mCtx.endSharedMeshBatch(batch.get());
            }
        }
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (chunks[i] != -1 && chunks[i] != CHUNK_BLOCK && chunks[i] != CHUNK_FLAT) direct(*items[i], false);
        }
        for (auto const* it : items) {
            float ratio = durabilityRatio(*it->stack);
            if (ratio < 0) continue;
            float x = it->x + 2 * it->scale, y = it->y + 13 * it->scale;
            fillRect({x, y, 13 * it->scale, 2 * it->scale}, Color::rgb(0x000000), Layer::Foreground);
            fillRect({x, y, 13 * it->scale * ratio, 1 * it->scale}, Color{1 - ratio, ratio, 0, 1}, Layer::Foreground);
        }
    }

    quads(Layer::Foreground);

    // Text.
    bool  anyText = false;
    auto& font    = uiFont(mCtx);
    for (auto& cmd : *mCmds) {
        auto* tc = std::get_if<TextCmd>(&cmd.v);
        if (!tc) continue;
        float w  = tc->width > 0 ? tc->width : textWidth(tc->s, tc->scale) + 2;
        float x0 = tc->x;
        auto  al = ::ui::TextAlignment::Left;
        if (tc->align == TextAlign::Center) al = ::ui::TextAlignment::Center;
        if (tc->align == TextAlign::Right) al = ::ui::TextAlignment::Right;
        if (tc->width <= 0 && tc->align == TextAlign::Center) x0 -= w / 2;
        if (tc->width <= 0 && tc->align == TextAlign::Right) x0 -= w;
        RectangleArea    area{x0, x0 + w, tc->y, tc->y + lineHeight(tc->scale) + 2};
        TextMeasureData  measure{tc->scale, 0.f, tc->c.r > 0.9f && tc->c.g > 0.9f, false, false, al};
        CaretMeasureData caret{-1, false};
        mCtx.drawText(font, area, std::string{tc->s}, toMce(tc->c), tc->c.a, al, measure, caret);
        anyText = true;
    }
    if (anyText) mCtx.flushText(0.f, std::nullopt);

    mCmds->clear();
}

} // namespace beetroot::ui

# AGENTS.md

BeetrootSkin is a port of [AppleSkin](https://github.com/squeek502/AppleSkin) (Java Edition food and
hunger HUD/tooltip info) to Minecraft Bedrock, built as a **client** mod on LeviLamina. It targets MC
**1.26.51.01** and LeviLamina **26.51.7**, and is built with xmake and clang-cl on Windows x64.
The project layout, rules and harness come from the sibling project BEI (same author, LGPL-3.0).

## Layout and ownership

| Path | What it is |
| ---- | ---------- |
| `src/core/` | Pure C++ logic, **no `mc/` or `ll/` includes**. Static lib `beetroot-core`, unit-tested on the host. `food/FoodMath` (Bedrock hunger/saturation/regen math, flash animation, AppleSkin sprite math), `food/HealthPreview` (health-restored overlay plan, `restingIconY`), `state/Config` (+`AtomicWrite`, `FileWatch`: live-reload change detection), `geom/Rect`, `geom/IconRows` (`RestingRows`: resting line of a shaking HUD icon bar). |
| `src/game/` | Adapters from game objects to core structs: `Game` (client/player/level helpers), `FoodValues` (item -> `core::FoodValues`), `PlayerFoodState` (local player -> `core::FoodState`, held food). |
| `src/ui/` | Rendering: `gfx/Painter`, `hud/HudHooks` (HUD layer contract + plumbing), `tooltip/TooltipHooks` (tooltip extension contract + plumbing), `Keys` (`isShiftDown()`). |
| `src/app/` | `Module` registry, `Mod` (paths, `config()`, `saveConfig()`, `pollConfigFile()`, `debugDraw()`), `ConfigWatch` (live config reload module), `I18n` (`tr()`), `KeyNames` (controls-menu names of BeetrootSkin keys). |
| `src/mod/` | Entry point (`BeetrootSkin.cpp`), `Log.h`, `MemoryOperators.cpp`. Installs every registered module; **features never edit it**. |
| `src/api/` | Implementation of the public API: registers the API events (`Api.cpp`, `onLoad()`), `Post.h` (`post`, `postCancellable`, cancellation logging). |
| `include/beetrootskin/api/` | **Public API headers** for other mods (AppleSkin's API): `FoodValues.h`, `Events.h`, `event/{FoodValuesEvent,TooltipOverlayEvent,HUDOverlayEvent}.h`, `Macros.h` (`BEETROOTSKIN_API`). Copied with `BeetrootSkin.lib` to `bin/BeetrootSkin-sdk/` at build time. |
| `samples/api_sample/` | Test-only mod using the API from another DLL (`xmake f --api_sample=y`, output `bin/beetroot-api-sample/`, e2e scenario `api`). Never shipped. |
| `src/debug/` | Debug HUD layer + tooltip extension, active only with the `debug_draw` marker (see below). |
| `src/features/<name>/` | One folder per feature (added by feature work). Picked up by the `src/**.cpp` glob. |
| `assets/lang/<locale>/*.json` | Translation fragments, merged into `bin/BeetrootSkin/lang/<locale>.json` at build time. Each feature owns its own fragment file; a duplicate key fails the build. |
| `assets/textures/**` | Mod-owned textures, copied to `bin/BeetrootSkin/textures/` at build time. `icons.png` and `tooltip_hunger_outline.png` are AppleSkin's (Unlicense, see `assets/textures/NOTICE`). |
| `tests/` | GoogleTest suites for `src/core` (target `beetroot-tests`). |
| `tools/e2e/` | PowerShell harness that drives the real game (see `tools/e2e/README.md`). |

## Contracts

These headers are **contracts** between features. Extend them compatibly (add members/overloads,
never change or remove existing ones); never edit `src/mod/`.

- `app/Module.h`: `beetroot::app::Module` (`name()`, `order()`, `install()`, `uninstall()`,
  `onLoad()`) and `BS_REGISTER_MODULE(obj)`. Everything that hooks or subscribes is a Module.
  `onLoad()` (default no-op) runs during `BeetrootSkin::load()` (via `app::detail::loadConfig()` ->
  `app::loadModules()`), before any `install()`: use it for things the game needs before mods are
  enabled, such as `ll::input::KeyRegistry` keys (see `features/debug_info`).
- `app/Mod.h`: `modDir()`, `configDir()`, `modFile(rel)`, `config()` (a `core::Config&`),
  `saveConfig()`, `pollConfigFile()`, `debugDraw()`. `config()` is replaced as a whole when the
  user edits config.json in game (`app/ConfigWatch.cpp` polls it after every UI render, stat about
  once a second, `core::FileChangeDetector`): read options where they are used, never cache them.
- `app/I18n.h`: `tr("beetroot.key", "English fallback")`.
- `core/food/FoodMath.h`: `FoodValues`, `FoodEffect`, `FoodState`, `canEat`, `eat`/`EatResult`,
  `hungerPreviewIcons`, `saturationOverlayIcons`, `healthPreviewIcons`, `isHalfIcon`,
  `naturalRegenEstimate`, `regenerationEffectHealth`, `estimatedHealthIncrement`,
  `estimatedHealthAfterEating`, `FlashAnimator`, `isHarmfulEffect`, `effect_id::*`,
  `sprites::{hudSaturationU, tooltipSaturationU, tooltipSaturationV, exhaustionBarWidth, ...}`.
- `core/food/DebugInfo.h`: `formatDecimal(value, DecimalPattern)` (java.text.DecimalFormat-like),
  `foodDebugLine(...)` (AppleSkin's debug text).
- `core/food/VanillaFoods.h`: `vanillaFoodEffects(itemId)`, `anyHarmful(effects)`.
- `core/state/Config.h`: `Config` with AppleSkin's option names.
- `core/geom/Rect.h`: `Rect` (GUI units).
- `core/geom/IconRows.h`: `RestingRows`, `jitterOffsets`.
- `game/Game.h`: `currentClient()`, `currentLevel()`, `currentPlayer()`, `integratedServerLevel()`.
- `core/food/ApiFoodValues.h`: `toApiFoodValues`, `withApiFoodValues` (core <-> public `api::FoodValues`).
- `game/FoodValues.h`: `getFoodValues(ItemStackBase const&, Player const* = nullptr)` (modified values,
  i.e. after `api::FoodValuesEvent`), `getDefaultFoodValues(stack)` (no event), `queryFoodValues(stack,
  player)` -> `QueriedFoodValues{defaultValues, modifiedValues}`.
- `game/PlayerFoodState.h`: `readFoodState()`, `HeldFood` (`values` = modified, `defaultValues`,
  `offhand`, `stack` valid for the current frame only), `heldFood()`.
- `include/beetrootskin/api/**`: the **public API** (see below). Other mods compile against it:
  never change or remove anything, only add (and bump `API_VERSION` only for incompatible changes).
- `ui/gfx/Painter.h`: `Painter`, `Color`, `palette::*`, `TextureRef` (`pack()`/`mod()`),
  `textures::ICONS`, `textures::TOOLTIP_HUNGER_OUTLINE`.
- `ui/hud/HudHooks.h`: `HudSlot`, `HudContext`, `IHudLayer`, `BS_REGISTER_HUD_LAYER(obj)`,
  `hudLayers()`, `clientTickCount()`, `hudFlashAlpha()`.
- `ui/tooltip/TooltipHooks.h`: `TooltipContext`, `ITooltipExtension`,
  `BS_REGISTER_TOOLTIP_EXTENSION(obj)`, `tooltipExtensions()`, `hoveredSlotItem()`.
- `ui/Keys.h`: `isShiftDown()`, `isControlDown()`, `isAltDown()`.

### HUD (`ui/hud/HudHooks.h`)

`HudHungerRenderer::$render` and `HudHeartRenderer::$render` are hooked (HUD scene only, i.e. the
current UI scene's name starts with `hud_screen`). For each draw, layers of slot `BelowHunger` /
`BelowHearts` run before the vanilla icons, `AboveHunger` / `AboveHearts` after them. Each layer
gets a fresh `HudContext`:

- `painter`: draws immediately (flushed after every layer);
- `slot`, `control` (owner UIControl rect);
- `hungerIcons[10]` (9x9 GUI rects, **index 0 = rightmost shank**, i.e. hunger 1-2, like
  AppleSkin's icon index), `hungerIconsValid`, `hungerJitterY[10]` (vanilla jitter offset, already
  included in the rects; subtract it to draw at rest when `showVanillaAnimationsOverlay` is off);
- `heartIcons` / `heartJitterY` (index 0 = leftmost heart of the bottom row; jitter = low-health shake);
- `state` (`readFoodState()`), `held` (`heldFood()`), `flashAlpha` (FlashAnimator x
  `maxHudOverlayFlashAlpha`, advanced on `ClientLevelTickEvent`, reset when nothing is held),
  `clientTick`.

Rects come from the renderers' `mIconPosition` (relative to the owner control's
`mCachedPosition`); verified in-game to match the vanilla icons at 1600x900, GUI scale 3.
Vanilla writes the shaken y straight into `mIconPosition` and keeps no resting layout, so the
jitter is measured against a resting line per row learned from still frames
(`core::RestingRows`, reset when the owner control moves; row 0 falls back to the control origin,
where both bars rest).

### Tooltips (`ui/tooltip/TooltipHooks.h`)

The hovered container slot is tracked like BEI (`ContainerScreenController` /
`CraftingScreenController` `$_onContainerSlotHovered`, unhover, tick, leave); its item is copied.
`HoverTextRenderer::$render` is hooked: when the hovered item has extensions with
`extraHeight() > 0`, the vanilla box (a `NinePatchLayer` built in `update()` and drawn at
`mCursorPosition + mOffset`) is stretched for that draw by the summed heights (+1 gap each) and
widened to the largest `extraWidth()` (+8 padding), moved up/left to stay on screen, drawn by
vanilla, restored, and then every extension's `render()` gets a `TooltipContext` with `painter`,
`stack`, `box` (whole box), `text` (vanilla text content rect) and `area` (its reserved rect, full
content width, stacked top to bottom in `order()`).

### Public API (`include/beetrootskin/api`)

AppleSkin's API as LeviLamina events on `ll::event::EventBus` (client thread). `src/api/Api.cpp`
registers every event with an empty emitter in `onLoad()` (LeviLamina rejects listeners for events
without an emitter; dependents subscribe in their `enable()`, which runs after BeetrootSkin's
`load()` when they declare the dependency). Identity across DLLs is each class's
`CustomEventId` string (`beetroot::api::...`), so the classes are header-only. Who posts what:

- `FoodValuesEvent`: `game::queryFoodValues` (so `getFoodValues`, `heldFood()`, the tooltip and
  every HUD preview use the modified values; tooltips lay out default vs modified).
- `TooltipOverlayEvent::Pre` in `features/tooltip` `query()` (cancel = no extension height),
  `::Render` in its `render()` (cancel = space kept, nothing drawn; x/y move it).
- `HUDOverlayEvent::Exhaustion` (exhaustion layer), `::Saturation` (saturation base layer, order 0;
  its outcome also gates/moves the gain layer, order 20, of the same draw), `::HungerRestored`
  (hunger preview), `::HealthRestored` (health preview, only when health would rise). Positions:
  right edge / resting top of the hunger bar; left edge / resting top of the bottom heart row.

Use `api_impl::postCancellable(ev)` (logs `API: <event id> cancelled by a listener` on changes)
when adding a cancellable event. Exported functions are marked `BEETROOTSKIN_API`
(BeetrootSkin is built with `BEETROOTSKIN_EXPORTS`).

### Mod-owned textures

`Painter::texture(TextureRef::mod("textures/icons.png"), dst, src, 256, 256, tint)` draws a
sub-rect of a PNG shipped in `bin/BeetrootSkin/textures/`. It is loaded with
`ResourceLocation(<absolute path>, ResourceFileSystem::Raw)` through
`MinecraftUIRenderContext::getTexture`; textures load asynchronously (a command for a texture
that is not loaded yet draws nothing; `Painter::textureReady()` tells). Verified in-game.

## Adding a feature

1. Create `src/features/<name>/` (namespace `beetroot::features::<name>`). It is compiled
   automatically.
2. Register what you need at namespace scope in your `.cpp`:
   - hooks/listeners: a `beetroot::app::Module` + `BS_REGISTER_MODULE(obj)`;
   - HUD drawing: an `ui::IHudLayer` + `BS_REGISTER_HUD_LAYER(obj)`;
   - tooltip content: an `ui::ITooltipExtension` + `BS_REGISTER_TOOLTIP_EXTENSION(obj)`.
3. Strings: `assets/lang/en_US/<name>.json` and `assets/lang/zh_CN/<name>.json` (keys
   `beetroot.<name>.*`), used via `app::tr()`.
4. Pure logic goes to `src/core/...` with tests in `tests/...` (no `mc/`/`ll/` there).
5. Add `tools/e2e/scenarios/<name>.ps1` (plus optional `.prelaunch.ps1` / `.restore.ps1`), run
   it, and look at its screenshots.
6. Add a line to `CHANGELOG.md` under `## [Unreleased]` / `### Added`.
7. Never edit `src/mod/` or change existing contract members. If a contract lacks something,
   add it compatibly (new member with a default, new function) in your branch.

## Build and test

New tools are on the persistent PATH but not in already-open shells, and LLVM is not on PATH at
all; in a fresh pwsh:
`$env:Path="C:\Program Files\LLVM\bin;"+[Environment]::GetEnvironmentVariable('Path','Machine')+';'+[Environment]::GetEnvironmentVariable('Path','User')`

```powershell
xmake f -a x64 -m release -p windows --target_type=client -y   # once (config is sticky)
xmake -y                                                         # bin/BeetrootSkin/{BeetrootSkin.dll,manifest.json,lang/,textures/}
xmake build -y beetroot-tests; xmake run beetroot-tests          # unit tests (src/core only)
xmake f --api_sample=y -y; xmake -y                              # + bin/beetroot-api-sample (e2e `api` only)
pwsh -NoProfile -File tools/e2e/run.ps1 -Scenario smoke          # real game, see tools/e2e/README.md
```

`git config --global core.longpaths true` is required (the LeviLamina package has very long header
paths). The levibuildscript mod packer reads the version from `git describe`, so the repository
needs at least one commit to build.

## Bedrock / LeviLamina rules (learned the hard way)

**Where the SDK headers are**
- `%LOCALAPPDATA%\.xmake\packages\l\levilamina\26.51.7\<hash>\include\{ll,mc}`; LeviLamina's own
  sources: `%LOCALAPPDATA%\lip\cache\git+https...LeviLamina.git#v26.51.7\{src,src-client}`.
- Check these before guessing any name or signature.

**What you may call**
- `MCAPI`, `MCFOLD` and `MCTAPI` functions and unmarked `virtual` functions are callable.
  **Never call `MCNAPI`.**
- `::ll::TypedStorage<A, S, T>` members: for class types use `mX.get()` / `*mX` / implicit
  conversion; for scalars, pointers, references and smart pointers the member **is** the plain
  type (`mPtr->...`, `mClientTexture.get()` is `shared_ptr::get`).
- Hook a virtual function through its `$name` thunk, e.g. `LL_TYPE_INSTANCE_HOOK(H,
  ll::memory::HookPriority::Normal, Cls, &Cls::$method, Ret, Args...)`; `H::hook()` /
  `H::unhook()` in your Module's install/uninstall. Put hooks in a named namespace, not an
  anonymous one (else clang warns about `_AutoHookCount`).
- `&Cls::$fn` of an MCAPI function (and `"..."_sym.resolve()`) points at a LeviLamina lazy import
  stub (`jmp [rip+0]` to another stub), never at the game code that vtables point at, so it
  cannot be used to identify a class by vtable. Use a layout check instead (see
  `game/FoodValues.cpp`). `ll::memory::getVtableIndex` cannot decode pure virtuals.

**Threads**
- UI rendering and `ClientLevelTickEvent` run on the client thread; `ServerLevelTickEvent` on the
  integrated server thread. Never touch the server level from the client thread:
  `PlayerFoodState` snapshots the server player's attributes in `ServerLevelTickEvent` and the
  client reads the snapshot (`FoodState::exact`).
- `KeyInputEvent` / `MouseInputEvent` arrive on the **window thread**: decide synchronously
  whether to `cancel()`, do the work on the client thread.

**Hotkeys**
- Create `KeyRegistry` keys in `Module::onLoad()` (too late in `install()`), bind them in no input
  mapping (`setInputMappingStack({})`), read the currently mapped codes in a
  `VanillaClientInputMappingFactory::$_updateKeyboardAndMouseControls` hook and match raw
  `KeyInputEvent`s against them (BEI's approach; `features/debug_info/DebugInfo.cpp`). The
  controls menu names a key `key.BeetrootSkin.<name>`; `app/KeyNames.cpp` answers those
  `I18nImpl::$get` lookups for every BeetrootSkin key from the lang keys `beetroot.key.<name>`
  and `beetroot.key.<name>.description`.

**Lifetimes**
- Never keep raw game pointers across frames. Copies of game objects in statics must not be
  destroyed during static destruction (leak them, see `TooltipHooks.cpp`) and are cleared on
  level exit / uninstall.

**Rendering**
- In the UI pass: rectangles/images -> `flushImages`, item icons, text -> `flushText`.
  `ui::Painter` does this; call `painter.flush()` between z bands.
- One `flushImages` call draws its quads with one texture: quads of several textures recorded
  into one batch draw partly/wrongly. `Painter` ends a batch whenever the texture or tint changes
  (submission order is kept), so layers can mix textures without flushing per quad.
- Coordinates are GUI units. Mouse pixels x `GuiData::mInvGuiScale` = GUI units.

**Food data**
- Bedrock saturation gain = nutrition x saturation_modifier x 2.
- `Item::getFood()` returns an `IFoodItemComponent`. Vanilla foods (verified: golden apple,
  rotten flesh) use `FoodItemComponentLegacy`, detected by layout (`mOwner` == the item,
  `mNutrition`/`mSaturationModifier` == the getters); its `mEffects` are the eat effects. The
  data-driven `FoodItemComponent` has no effect list; then (and for legacy components without
  effects) `core::vanillaFoodEffects(itemId)` supplies vanilla values.
- Attributes: `Player::HUNGER()/SATURATION()/EXHAUSTION()`, `SharedAttributes::HEALTH()`,
  `actor.getAttribute(attr).mPtr->mCurrentValue` (and `mCurrentMaxValue`).
- The server syncs `minecraft:player.hunger`, `.saturation`, `.exhaustion` and `minecraft:health`
  to the client (UpdateAttributesPacket, on every change; verified with `force_client_sync`), and
  the client level gets difficulty (SetDifficultyPacket) and game rules (GameRulesChangedPacket), so
  the client-attribute path is accurate on remote servers too; no exhaustion estimate is needed
  (the client also predicts exhaustion locally: it can briefly read above 4 until the server's
  saturation/hunger step arrives one tick later).
- Verified against a real **Bedrock Dedicated Server 1.26.51.1** (official build matching the
  1.26.51.01 client; e2e scenario `remote`, see `tools/e2e/README.md`): the food state comes from
  the client attributes (`source=client`, `exact=false`) and follows server-side `/effect hunger`,
  `/damage`, eating (cooked beef: +8 hunger, saturation +12.8 capped at hunger), sprinting
  (exhaustion), Health Boost (max health 40, two heart rows), `/gamerule naturalregeneration false`
  (GameRulesChangedPacket carries only the changed rule; health preview hides) and `/difficulty
  peaceful` (SetDifficultyPacket). Tooltips, previews, saturation overlay, exhaustion underlay and
  the debug line (`(client)`) all work there. On a remote server the client level ticks and the
  first UpdateAttributes/GameRulesChanged/SetDifficulty packets arrive **before**
  `ClientJoinLevelEvent`: don't reset per-session state on join (reset on exit).
- Look game rules up by name (`GameRule::mName`, e.g. `naturalRegeneration`, case-insensitive),
  not by `GameRulesIndex`: index 20 (`DoNaturalRegeneration`) read `true` after
  `/gamerule naturalregeneration false` on 1.26.51.

**Policy**
- Lamium (github.com/amatouhake/Lamium) has no license. Read it for ideas only and never copy code.

## Debug drawing

If `mods/BeetrootSkin/debug_draw` exists at load (`app::debugDraw()`), `src/debug/DebugDraw.cpp`
outlines every hunger/heart icon rect, draws AppleSkin's saturation sprite and exhaustion strip
from `icons.png` on the hunger bar, reserves a magenta 10-unit area in every item tooltip and logs
held/hovered food values. The e2e `smoke` scenario creates and removes the marker.

## Conventions

- C++20, `.clang-format` (LLVM-based, 4 spaces, 120 columns). Namespaces follow directories:
  `beetroot::core`, `beetroot::game`, `beetroot::ui`, `beetroot::app`, `beetroot::features::<name>`.
- English in code and comments. User-facing strings go through `app::tr()` and the lang fragments
  (`en_US` and `zh_CN` both required).
- Log with `beetroot::log()` (`mod/Log.h`). No per-frame logging; log on state changes.
- Every `src/core` change comes with gtest coverage in `tests/`.
- Run `clang-format -i` on changed `src/` and `tests/` files before committing.

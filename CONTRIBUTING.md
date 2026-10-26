# Contributing to BeetrootSkin

Thanks for helping! Bug reports, ideas and pull requests are all welcome.

## Reporting bugs and requesting features

Use the [issue templates](https://github.com/LiteLDev/BeetrootSkin/issues/new/choose). For bugs, include:

- the BeetrootSkin version;
- the LeviLamina version;
- the Minecraft version;
- whether it happened in single-player (or a LAN world you host) or on a server;
- steps to reproduce;
- the relevant part of `logs/latest.log` from your game folder.

If a number looks wrong, compare it with what [AppleSkin](https://github.com/squeek502/AppleSkin) shows for the same food on Java Edition, and keep in mind the Bedrock differences listed under [How it works](#how-it-works).

## Development setup

See [Development](#development) below. [AGENTS.md](AGENTS.md) is the developer guide. It covers the layout of the code, the contracts between modules, threading and lifetime rules, and how to read the LeviLamina SDK headers. Please read it before changing anything under `src/`.

## Pull requests

1. Fork the repository and create a branch from `main`.
2. Keep each pull request focused on one change. Describe what it does and how you tested it.
3. Follow the existing style: C++20, the provided `.clang-format` and `.editorconfig`, English in code and comments.
4. User-facing text goes through `app::tr` with entries in **both** `assets/lang/en_US/` and `assets/lang/zh_CN/`.
5. Logic in `src/core` must stay free of game headers and come with unit tests in `tests/`:
   `xmake build -y beetroot-tests; xmake run beetroot-tests`.
6. If you change something visible in game, run the relevant end-to-end scenario from [tools/e2e](tools/e2e/README.md) and check its screenshots. Better still, add a scenario.
7. CI must pass before a pull request is merged.

Commit messages use the imperative mood ("Add exhaustion underlay", not "Added …"). Update [CHANGELOG.md](CHANGELOG.md) under *Unreleased* for user-visible changes.

## Development

### Building

You need Windows x64, [xmake](https://xmake.io), Visual Studio Build Tools with MSVC 14.4x or newer, and LLVM (`clang-cl`).

```powershell
git config --global core.longpaths true
xmake f -a x64 -m release -p windows --target_type=client -y
xmake -y                                                  # produces bin/BeetrootSkin/
xmake build -y beetroot-tests; xmake run beetroot-tests   # unit tests
```

The mod version comes from `git describe`, so build from a git clone with at least one commit. [AGENTS.md](AGENTS.md) describes the architecture, the conventions and the rules for working with Bedrock internals.

### Tests

- Unit tests for the pure logic in `src/core`: `xmake build -y beetroot-tests; xmake run beetroot-tests`.
- [tools/e2e](tools/e2e/README.md) is the end-to-end test harness that drives the real game in the background.

### Showcase media

The screenshots and videos in [docs/showcase](docs/showcase/) (the README and the [LeviMC Mods gallery](https://levimc.org/mods/github.com/LiteLDev/BeetrootSkin/?tab=gallery), listed in `docs/showcase/showcase.json`) come from the end-to-end `showcase` scenario:

```powershell
pwsh -NoProfile -File tools/e2e/run.ps1 -Scenario showcase
```

It writes raw captures to `tools/e2e/out/showcase/`; the scenario's header says how they are turned into the WebP/MP4 files.

### How it works

- **Regeneration.** Bedrock heals at a fixed rate while hunger is at least 18 and has no Java-style saturation-boosted fast regeneration. The health preview uses Bedrock's rules, so it can differ from what AppleSkin shows for the same food on Java. Saturation gained is `nutrition × saturation modifier × 2`, as in Bedrock.
- **Where the values come from.** In single-player and when you host a LAN world, the values come straight from the integrated server and are exact. On dedicated servers and joined worlds, BeetrootSkin uses the hunger, saturation and exhaustion attributes the server sends to the client, which can lag slightly behind the server's own values.
- **Config file handling.** `config.json` is polled about once a second (the log shows `Config reloaded: ...` with the changed options). If it is not valid JSON, the current settings are kept, a warning is logged and the file is left as is. Missing keys get their defaults, and unknown keys are kept when the file is saved. When the mod saves the file itself (for example after `F6`) while it holds content the mod could not read, that content is first copied to `config.json.bak`. `version` is managed by the mod.

### API for other mods

Other LeviLamina mods can integrate with BeetrootSkin through a C++ port of [AppleSkin's API](https://github.com/squeek502/AppleSkin/tree/1.21.11-neoforge/java/squeek/appleskin/api). The events are LeviLamina events on `ll::event::EventBus`, posted on the client thread:

| Event | When | Cancel to |
| --- | --- | --- |
| `beetroot::api::FoodValuesEvent` | BeetrootSkin determines the food values of an item (tooltips and the held food behind every HUD preview). Change `modifiedFoodValues()` (or `defaultFoodValues()`): every display uses the modified values, and tooltips show default vs modified with AppleSkin's outlines. | (not cancellable) |
| `beetroot::api::TooltipOverlayEvent::Pre` | Food values are about to be added to a tooltip. | show no food values for this item |
| `beetroot::api::TooltipOverlayEvent::Render` | They are about to be drawn (`x()`/`y()` movable, `renderContext()` to draw your own). | keep the space, draw nothing |
| `beetroot::api::HUDOverlayEvent::Exhaustion` | The exhaustion underlay is drawn (`exhaustion()`). | hide the underlay |
| `beetroot::api::HUDOverlayEvent::Saturation` | The saturation overlay is drawn (`saturationLevel()`). | hide the saturation overlay and the held food's saturation gain |
| `beetroot::api::HUDOverlayEvent::HungerRestored` | The held food's hunger preview is drawn (`currentFoodLevel()`, `itemStack()`, `foodValues()`). | hide the hunger preview |
| `beetroot::api::HUDOverlayEvent::HealthRestored` | The held food's health preview is drawn (`modifiedHealth()`, `itemStack()`, `foodValues()`). | hide the health preview |

HUD events carry the overlay position in GUI units (`x()`/`y()`, changeable to move it): the right edge and top of the hunger bar, or the left edge and bottom-row top of the hearts. `beetroot::api::FoodValues` holds `hunger`, `saturation` (saturation gained) and `canAlwaysEat`. Two functions are exported as well: `apiVersion()` and `queryFoodValues(stack, player)` (default and modified values of an item, firing `FoodValuesEvent`).

To use it:

1. Get the SDK: `beetrootskin-sdk-client-windows-x64.zip` from a release, or build BeetrootSkin, which writes it to `bin/BeetrootSkin-sdk/`. It holds `include/beetrootskin/api/*.h` and `lib/BeetrootSkin.lib`.
2. Add the SDK's `include` to your include path and link `BeetrootSkin.lib` (the headers also need LeviLamina's).
3. Declare the dependency in your `manifest.json`, so that BeetrootSkin loads (and registers its events) before your mod: `"dependencies": [{ "name": "BeetrootSkin" }]`.
4. Subscribe in your mod's `enable()`:

```cpp
#include "beetrootskin/api/Events.h"

#include "ll/api/event/EventBus.h"
#include "mc/world/item/ItemStackBase.h"

// In your mod's enable():
auto& bus = ll::event::EventBus::getInstance();
mListeners.push_back(bus.emplaceListener<beetroot::api::FoodValuesEvent>([](beetroot::api::FoodValuesEvent& ev) {
    if (ev.itemStack().getTypeName() == "minecraft:bread") ev.modifiedFoodValues().hunger *= 2;
}));
mListeners.push_back(bus.emplaceListener<beetroot::api::HUDOverlayEvent::Exhaustion>(
    [](beetroot::api::HUDOverlayEvent::Exhaustion& ev) { ev.cancel(); }
));
mListeners.push_back(bus.emplaceListener<beetroot::api::TooltipOverlayEvent::Pre>(
    [](beetroot::api::TooltipOverlayEvent::Pre& ev) {
        if (ev.itemStack().getTypeName() == "minecraft:apple") ev.cancel();
    }
));
// In disable(): bus.removeListener(l) for each of them.
```

[samples/api_sample](samples/api_sample/ApiSample.cpp) is a complete mod that does exactly this; it is built with `xmake f --api_sample=y` and used by the `api` end-to-end scenario.

## License

By contributing you agree that your contributions are licensed under the [LGPL-3.0-or-later](COPYING.LESSER), the license of this project.

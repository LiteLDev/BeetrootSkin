# BeetrootSkin

<img src="docs/logo.png" alt="" width="128" align="right">

English | [简体中文](README.zh-CN.md)

[![standard-readme compliant](https://img.shields.io/badge/readme%20style-standard-brightgreen.svg)](https://github.com/RichardLitt/standard-readme)
[![Build](https://github.com/LiteLDev/BeetrootSkin/actions/workflows/build.yml/badge.svg)](https://github.com/LiteLDev/BeetrootSkin/actions/workflows/build.yml)
[![Release](https://img.shields.io/github/v/release/LiteLDev/BeetrootSkin)](https://github.com/LiteLDev/BeetrootSkin/releases)
[![License](https://img.shields.io/github/license/LiteLDev/BeetrootSkin)](COPYING.LESSER)

AppleSkin-style food and hunger information for Minecraft Bedrock Edition, as a LeviLamina client mod.

BeetrootSkin brings what [AppleSkin](https://github.com/squeek502/AppleSkin) does on Java Edition to Bedrock: it shows how much hunger and saturation a food restores in its tooltip, previews the hunger, saturation and health you would gain while you hold food, and makes the hidden saturation and exhaustion values visible on the HUD. It runs entirely on the client, so the server does not need to install anything.

## Table of Contents

- [Background](#background)
- [Features](#features)
- [Install](#install)
- [Usage](#usage)
- [Configuration](#configuration)
- [API](#api)
- [Building](#building)
- [Maintainers](#maintainers)
- [Contributing](#contributing)
- [License](#license)

## Background

Minecraft tracks more about hunger than the ten shanks on the HUD show. Saturation is spent before hunger and decides how long a full bar lasts, and exhaustion builds up with every action until it drains a point of saturation or hunger. Food tooltips say nothing about how much a food is worth. On Java Edition, [AppleSkin](https://github.com/squeek502/AppleSkin) by [squeek502](https://github.com/squeek502) has made all of this visible for years. BeetrootSkin ports it to Bedrock, built on [LeviLamina](https://github.com/LiteLDev/LeviLamina), which supports Bedrock clients on Windows.

The HUD and tooltip sprites (`assets/textures/icons.png` and `tooltip_hunger_outline.png`) are AppleSkin's own, released by squeek502 under the [Unlicense](https://unlicense.org) (see [assets/textures/NOTICE](assets/textures/NOTICE)). Thanks to squeek502 for AppleSkin and for putting its art in the public domain. The code is a rewrite for Bedrock.

## Features

Every AppleSkin feature is available, with the same option names:

- **Food values in tooltips**: hover a food in any inventory to see the hunger (shanks) and saturation it restores, drawn with AppleSkin's icons. Rotten flesh, raw chicken and other foods with harmful effects use the "bad food" icons. Shown always by default, or only while `Shift` is held.
- **Hunger and saturation restored preview**: while you hold food in your main hand (or offhand), the hunger bar flashes the shanks and saturation that eating it would add.
- **Saturation overlay**: your current saturation is drawn as golden outlines over the hunger bar.
- **Exhaustion underlay**: a bar under the hunger bar fills up as exhaustion builds toward the next point of saturation or hunger being used.
- **Health restored preview**: while you hold food, the hearts flash the health you would regain from eating it: natural regeneration from the new hunger and saturation, plus the Regeneration effect of foods such as golden apples.
- **Debug info**: a line on the HUD with your exact hunger, saturation and exhaustion values.
- **Vanilla animations**: the overlays follow the vanilla hunger bar's jitter, so they stay on top of the icons they belong to.

Bedrock-specific notes:

- **No F3 screen.** Bedrock has no Java-style F3 debug screen, so the debug info is a separate line on the HUD instead. Press **`F6`** in game to show or hide it (saved as `showFoodDebugInfo`).
- **Bedrock's regeneration model.** Bedrock heals at a fixed rate while hunger is at least 18 and has no Java-style saturation-boosted fast regeneration. The health preview uses Bedrock's rules, so it can differ from what AppleSkin shows for the same food on Java. Saturation gained is `nutrition × saturation modifier × 2`, as in Bedrock.
- **Client-only.** Nothing is installed on the server. In single-player and when you host a LAN world, the values come straight from the integrated server and are exact. On dedicated servers and joined worlds, BeetrootSkin uses the hunger, saturation and exhaustion attributes the server sends to the client, which can lag slightly behind the server's own values.

## Install

Requirements:

- Minecraft Bedrock Edition **1.26.51.01** for Windows (x64).
- LeviLamina **26.51.x** client, for example installed through [LeviLauncher](https://github.com/LiteLDev/LeviLauncher).

Then install BeetrootSkin into that game instance in one of these ways.

With [lip](https://github.com/futrime/lip):

```sh
lip install github.com/LiteLDev/BeetrootSkin#client
```

Manually: download `beetrootskin-client-windows-x64.zip` from the [latest release](https://github.com/LiteLDev/BeetrootSkin/releases/latest) and extract it so that `BeetrootSkin/` ends up in the game's `mods/` folder, next to `LeviLamina/`.

## Usage

There is nothing to set up. Start a world in survival or adventure mode:

- hover a food in your inventory to see its values in the tooltip;
- hold a food to preview the hunger, saturation and health it restores;
- watch the saturation outline and the exhaustion bar on the hunger bar as you play;
- press `F6` to show or hide the debug line.

## Configuration

The settings are stored in `mods/BeetrootSkin/config/config.json`. The file is created with the defaults on first launch. Edit it at any time, even while the game is running: changes are applied within about a second of saving the file, no restart needed (the log shows `Config reloaded: ...` with the changed options). If the file is not valid JSON, the current settings are kept, a warning is logged and the file is left as is for you to fix. The option names are AppleSkin's, so its documentation applies.

| Key | Default | Meaning |
| --- | --- | --- |
| `showFoodValuesInTooltip` | `true` | Show the hunger and saturation icons in food tooltips while `Shift` is held. |
| `showFoodValuesInTooltipAlways` | `true` | Show them even without `Shift` (like AppleSkin, this works on its own). |
| `showSaturationHudOverlay` | `true` | Draw the current saturation as an outline over the hunger bar. |
| `showFoodValuesHudOverlay` | `true` | Flash the hunger and saturation the held food would restore on the hunger bar. |
| `showFoodValuesHudOverlayWhenOffhand` | `true` | Also preview food held in the offhand. |
| `showFoodExhaustionHudUnderlay` | `true` | Draw the exhaustion bar under the hunger bar. |
| `showFoodHealthHudOverlay` | `true` | Flash the health the held food would restore on the hearts. |
| `showVanillaAnimationsOverlay` | `true` | Make the overlays follow the vanilla hunger bar jitter. |
| `maxHudOverlayFlashAlpha` | `0.65` | Peak opacity of the flashing previews, from `0` (invisible) to `1`. Out-of-range values are clamped. |
| `showFoodDebugInfo` | `false` | Show the debug line with exact hunger, saturation and exhaustion. Toggled in game with `F6`. |
| `version` | `1` | Config format version, managed by the mod. Do not edit. |

Missing keys get their defaults, and keys this version does not know are kept when the file is saved. When the mod saves the file itself (for example after `F6`) while it holds content the mod could not read, that content is first copied to `config.json.bak`.

## API

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

## Building

You need Windows x64, [xmake](https://xmake.io), Visual Studio Build Tools with MSVC 14.4x or newer, and LLVM (`clang-cl`).

```powershell
git config --global core.longpaths true
xmake f -a x64 -m release -p windows --target_type=client -y
xmake -y                                                  # produces bin/BeetrootSkin/
xmake build -y beetroot-tests; xmake run beetroot-tests   # unit tests
```

The mod version comes from `git describe`, so build from a git clone with at least one commit. [AGENTS.md](AGENTS.md) describes the architecture, the conventions and the rules for working with Bedrock internals. [tools/e2e](tools/e2e/README.md) is the end-to-end test harness that drives the real game.

## Maintainers

[@futrime (Zijian Zhang)](https://github.com/futrime)

## Contributing

PRs and issues are welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md) and follow the [Code of Conduct](CODE_OF_CONDUCT.md). Report security issues as described in [SECURITY.md](SECURITY.md).

## License

LGPL-3.0 © 2026 Zijian Zhang

BeetrootSkin is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License version 3, the same license as LeviLamina. See [COPYING.LESSER](COPYING.LESSER) and [COPYING](COPYING).

The textures in `assets/textures/` are from AppleSkin by squeek502 and are released under the Unlicense. See [assets/textures/NOTICE](assets/textures/NOTICE).

BeetrootSkin is not affiliated with Mojang Studios, Microsoft, or the AppleSkin project.

BeetrootSkin is not affiliated with Mojang Studios, Microsoft, or the AppleSkin project.

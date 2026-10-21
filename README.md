# BeetrootSkin

AppleSkin-style food and hunger information for Minecraft Bedrock Edition, as a [LeviLamina](https://github.com/LiteLDev/LeviLamina) client mod.

BeetrootSkin is a port of [AppleSkin](https://github.com/squeek502/AppleSkin) (Java Edition) to Bedrock: it shows the hunger and saturation a food restores in its tooltip, previews the gain on the HUD while you hold food, and shows your hidden saturation and exhaustion. It runs entirely on the client.

## Install

```sh
lip install github.com/LiteLDev/BeetrootSkin#client
```

Requires Minecraft Bedrock 1.26.51 with LeviLamina 26.51 (client).

## Configuration

`mods/BeetrootSkin/config/config.json` uses AppleSkin's option names (`showFoodValuesInTooltip`, `showSaturationHudOverlay`, `showFoodExhaustionHudUnderlay`, `maxHudOverlayFlashAlpha`, ...).

## Building

See [AGENTS.md](AGENTS.md#build-and-test). In short: `xmake f -a x64 -m release -p windows --target_type=client -y && xmake -y`.

## License

LGPL-3.0-or-later, see [COPYING](COPYING) and [COPYING.LESSER](COPYING.LESSER). The textures in `assets/textures/` come from AppleSkin (Unlicense), see `assets/textures/NOTICE`.

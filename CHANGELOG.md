# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Project scaffold: LeviLamina client mod build, configuration file (AppleSkin option names), Bedrock food math with unit tests, HUD and tooltip extension points, and an end-to-end test harness.
- Exhaustion underlay behind the hunger bar (AppleSkin's `showFoodExhaustionHudUnderlay`), hidden in creative/spectator and whenever vanilla hides the hunger bar (Bedrock keeps it while riding and draws the mount's hearts above it).
- Food-state accuracy (AppleSkin's sync equivalent): the integrated-server snapshot survives pauses and is cleared on leaving a world, the naturalregeneration game rule is looked up by name (the index lookup always read `true`), and source changes, synced attributes and rule/difficulty updates are logged; `mods/BeetrootSkin/force_client_sync` forces the remote-server (client attribute) path for testing.
- Food tooltips: hunger and saturation icons for food items (hold Shift, or always), like AppleSkin.
- Hunger-restored HUD preview: the shanks the held food (main hand, or offhand) would fill flash on the hunger bar, green for rotten food (AppleSkin `showFoodValuesHudOverlay`).
- Food debug info: AppleSkin's `hunger: N, sat: #.##, exh: 0.00/4` line, drawn at the top left of the HUD (above the vanilla position/day text and chat) when `showFoodDebugInfo` is on, and a remappable hotkey (default F6, listed in Settings > Keyboard & Mouse) that toggles and saves it.
- Health-restored HUD preview: while holding food, the hearts that eating it would restore (Bedrock natural regeneration after eating plus the food's Regeneration effect) flash over the health bar, like AppleSkin's HealthRestored overlay.
- Saturation HUD overlay: golden outlines over the hunger shanks show the current saturation, and the saturation the held food would add flashes on top of them.
- End-to-end harness: `controls` / `controls_zh` scenarios check the food debug info key in Settings > Keyboard & Mouse (localized name and default F6, English and Chinese), pause/settings navigation helpers, focus-steal recovery (re-focus retries with a log of the window that held the foreground), waiting out a locked workstation instead of failing, pause-menu ESC retries, restore of other mods and `options.txt` after an interrupted run, and `tools/e2e/restore-mods.ps1` for manual recovery.
- Public API for other LeviLamina mods, a port of AppleSkin's: `FoodValuesEvent` (change the food values every display uses; tooltips show default vs modified), `TooltipOverlayEvent::{Pre, Render}` and `HUDOverlayEvent::{Exhaustion, Saturation, HungerRestored, HealthRestored}` (cancellable, movable), as LeviLamina events; headers and import library in `bin/BeetrootSkin-sdk/`.
### Fixed

- HUD overlays follow (or, with `showVanillaAnimationsOverlay` off, ignore) the vanilla hunger-bar shake correctly: the jitter is measured from a resting line learned from still frames instead of the highest shank of the frame (wrong while every shank shakes at saturation 0).
- Heart jitter (vanilla low-health shake) is now reported to HUD layers, so the health preview honors `showVanillaAnimationsOverlay`.
- Textured quads of different textures recorded between two flushes all draw correctly (the Painter splits its batches per texture); the food tooltip no longer flushes after every quad.
- On Peaceful, food counts as edible even with a full hunger bar, like Bedrock: the saturation gain preview shows for a full bar (the health preview stays hidden on Peaceful, hunger/saturation previews in Creative).

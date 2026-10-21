# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Project scaffold: LeviLamina client mod build, configuration file (AppleSkin option names), Bedrock food math with unit tests, HUD and tooltip extension points, and an end-to-end test harness.
- Exhaustion underlay behind the hunger bar (AppleSkin's `showFoodExhaustionHudUnderlay`), hidden in creative/spectator and while a mount's health replaces the hunger bar.
- Food tooltips: hunger and saturation icons for food items (hold Shift, or always), like AppleSkin.
- Hunger-restored HUD preview: the shanks the held food (main hand, or offhand) would fill flash on the hunger bar, green for rotten food (AppleSkin `showFoodValuesHudOverlay`).
- Food debug info: AppleSkin's `hunger: N, sat: #.##, exh: 0.00/4` line, drawn at the top left of the HUD (above the vanilla position/day text and chat) when `showFoodDebugInfo` is on, and a remappable hotkey (default F6, listed in Settings > Keyboard & Mouse) that toggles and saves it.
- Health-restored HUD preview: while holding food, the hearts that eating it would restore (Bedrock natural regeneration after eating plus the food's Regeneration effect) flash over the health bar, like AppleSkin's HealthRestored overlay.
- Saturation HUD overlay: golden outlines over the hunger shanks show the current saturation, and the saturation the held food would add flashes on top of them.

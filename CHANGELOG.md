# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Project scaffold: LeviLamina client mod build, configuration file (AppleSkin option names), Bedrock food math with unit tests, HUD and tooltip extension points, and an end-to-end test harness.
- Food debug info: AppleSkin's `hunger: N, sat: #.##, exh: 0.00/4` line, drawn at the top left of the HUD (above the vanilla position/day text and chat) when `showFoodDebugInfo` is on, and a remappable hotkey (default F6, listed in Settings > Keyboard & Mouse) that toggles and saves it.

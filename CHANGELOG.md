# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Project scaffold: LeviLamina client mod build, configuration file (AppleSkin option names), Bedrock food math with unit tests, HUD and tooltip extension points, and an end-to-end test harness.
- Food-state accuracy (AppleSkin's sync equivalent): the integrated-server snapshot survives pauses and is cleared on leaving a world, the naturalregeneration game rule is looked up by name (the index lookup always read `true`), and source changes, synced attributes and rule/difficulty updates are logged; `mods/BeetrootSkin/force_client_sync` forces the remote-server (client attribute) path for testing.

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

If a number looks wrong, compare it with what [AppleSkin](https://github.com/squeek502/AppleSkin) shows for the same food on Java Edition, and keep in mind the Bedrock differences listed in the [README](README.md#features).

## Development setup

See [Building](README.md#building) in the README. [AGENTS.md](AGENTS.md) is the developer guide. It covers the layout of the code, the contracts between modules, threading and lifetime rules, and how to read the LeviLamina SDK headers. Please read it before changing anything under `src/`.

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

## License

By contributing you agree that your contributions are licensed under the [LGPL-3.0](COPYING.LESSER), the license of this project.

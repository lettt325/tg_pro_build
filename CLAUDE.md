# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repository is

A **personal, non-public fork** of [Telegram Desktop](https://github.com/telegramdesktop/tdesktop). It builds a customized macOS client packaged as a DMG (app name **`TGPro`**, set at `Telegram/CMakeLists.txt` `output_name`). It exists to host private features layered on top of upstream tdesktop (e.g. a parasitic-/filler-word detector and similar personal tools). Only the macOS DMG target is maintained here; Windows/Linux packaging is inherited from upstream but not exercised.

## Building

This client is built **on CI, not locally** — there is no Mac on the dev box. The build runs as a GitHub Actions workflow on a Depot macOS runner and produces `TGPro.dmg`.

- **Trigger a build:** push to the `dev` branch, or `gh workflow run build_pro.yml --ref dev`.
- **Pushing to `dev` also republishes the `nightly` GitHub release** and uploads the DMG/.app artifacts — so a push is outward-facing, not a private build.
- **Read `BUILD_GUIDE.md` before touching anything build-related.** It captures the build pipeline and the (hard-won) caching setup so you don't re-investigate it. Do not reintroduce static ccache keys, build-dir caching, or git-timestamp hacks — `BUILD_GUIDE.md` explains why they break things.

**Speed depends on what you edit:** changing a `.cpp` recompiles only that file (~11 min build, ~99% ccache hit). Changing a header that the precompiled header pulls in (`Telegram/SourceFiles/stdafx.h` or the generated `scheme.h` / `palette.h` / `style_basic.h`, or widely-included `base/` `ui/` `data/` headers) forces a near-full rebuild of ~1480 files. Keep declarations in hot headers stable; put implementation in `.cpp`.

## Architecture orientation (what takes several files to see)

- **Build-time code generation feeds the PCH.** `scheme.h` is generated from the TL schema (`*.tl`), `palette.h`/`style_basic.h` from `.style` files, `lang_auto.h` from `.strings`. These generated headers are `#include`d by the PCH, which is why regenerating/editing them is expensive. Generators live under `Telegram/SourceFiles/codegen/` and `Telegram/cmake/`.
- **Modular CMake.** The app is assembled from sub-libraries defined in `Telegram/cmake/*.cmake` (e.g. `td_ui`, `td_mtproto`, `td_iv`, `td_export`, `td_lang`), each with its own PCH. The final `Telegram`/`TGPro` target links them in `Telegram/CMakeLists.txt`.
- **Feature code lives under `Telegram/SourceFiles/<area>/`** — `history/`, `data/`, `ui/`, `window/`, `settings/`, `chat_helpers/`, `info/`, `calls/`, `media/`, etc. New personal features generally go in a new area dir (or extend an existing one) plus wiring in the relevant `*.cmake` source list.
- **Pro Settings** live in `settings/sections/settings_pro.cpp` (UI) and `settings/pro/pro_settings_storage.h/.cpp` (persistent storage via the encrypted per-account pref system). Toggle states, weak words list, and exception peer IDs are serialized as JSON to disk.
- **Sparkle auto-update** — `platform/mac/sparkle_mac.h/.mm` wraps Sparkle 2 (gated by `TDESKTOP_USE_SPARKLE`, scoped to that one `.mm` file to avoid ccache invalidation). CMake option `DESKTOP_APP_DISABLE_SPARKLE` (ON by default, OFF in CI). Ed25519 keys are GitHub secrets, appcast.xml is generated per build.
- **App identity / API creds** are this fork's customizations: `output_name "TGPro"` in `Telegram/CMakeLists.txt`; `TDESKTOP_API_ID` / `TDESKTOP_API_HASH` are passed by the build workflow, not hardcoded in source.

## Upstream conventions

`AGENTS.md` is the upstream tdesktop agent guide. It is **Windows/WSL-oriented** and its build instructions do **not** apply to this fork — use `BUILD_GUIDE.md` for builds. Consult `AGENTS.md` only for general upstream code conventions (line endings, code style).

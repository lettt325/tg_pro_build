# BUILD_GUIDE.md — how the TGPro macOS build works

Quick general knowledge so you don't re-investigate the build/caching. Pairs with `CLAUDE.md`.

## TL;DR

- Built on **CI only** (GitHub Actions → Depot macOS runner), workflow `.github/workflows/build_pro.yml`. Output: `TGPro.dmg`.
- **CMake + Ninja**, compiled `-O0 -g0` (fast build, unoptimized — fine for a nightly client).
- **Incremental speed comes entirely from ccache.** A warm build is **~11 min at ~99% cache hit**. A cold/first build is ~30–40 min.
- Trigger: push to `dev`, or `gh workflow run build_pro.yml --ref dev`. A push **also republishes the `nightly` GitHub release** + uploads DMG/.app artifacts.

## Pipeline (steps in `build_pro.yml`)

1. `brew install` toolchain + Qt + ffmpeg@6 etc.; select Xcode.
2. **Download Sparkle 2** framework (for auto-updates, see below).
3. Restore prebuilt **dependencies** (RNNoise, WebRTC `tg_owt`, TDE2E) via `actions/cache`, keyed on each dep's upstream Git SHA. Cache hit ⇒ skip building them (they rarely change).
4. Restore **ccache** (the real workhorse — see below).
5. `cmake -B build` configure with the fork's flags + `TDESKTOP_API_ID/HASH` + `-D DESKTOP_APP_DISABLE_SPARKLE=OFF`.
6. `ninja -C build -j $(sysctl -n hw.logicalcpu)` — full graph (~1800 targets); ccache makes unchanged files instant.
7. `macdeployqt` → ad-hoc `codesign` → `hdiutil` DMG.
8. **Generate `appcast.xml`** — sign DMG with Ed25519 key, commit to repo.
9. Publish `nightly` release + upload artifacts.

## Caching — the critical part (do not regress)

Incremental builds rely on **ccache only**. Each CI run is a fresh ephemeral macOS VM with a fresh checkout, so ninja sees every file as "new" (mtimes = now) and re-invokes the compiler for all ~1800 TUs — **ccache returns the cached `.o` for unchanged files in milliseconds**, so only what actually changed compiles.

Four things make it work, all currently in place:

1. **Rolling ccache key with clang version** `macOS-ccache-clang<ver>-${{ github.run_id }}` + `restore-keys: macOS-ccache-clang<ver>-`. Each run saves a fresh cache and restores the newest prior one. A **static key freezes the cache** (`actions/cache` skips saving on an exact key hit) — that was an early bug; never use a fixed key. The clang version in the key ensures a real compiler update creates a new cache.
2. **`-Xclang -fno-pch-timestamp`** (on `CMAKE_CXX_FLAGS_INIT` and `CMAKE_OBJCXX_FLAGS_INIT`). **This is load-bearing.** Telegram uses a big precompiled header (`stdafx.h`). Clang embeds input-header timestamps into the compiled `.pch`; on a fresh runner those mtimes differ every run, so the `.pch` binary differs, and since ccache hashes the `.pch` content, **every one of ~1480 PCH-dependent C++ files misses** — hit rate collapses to ~17%. The flag stops clang embedding timestamps → `.pch` is byte-reproducible → ~99% hits.
3. **`compiler_check=string:<clang version>`** instead of `compiler_check=content`. **This is load-bearing.** With `content`, ccache hashes the ccache wrapper binary at `/opt/homebrew/opt/ccache/libexec/c++`. When brew updates ccache or its dependency hiredis, the wrapper binary changes → all cache entries get a different compiler hash → 95% miss rate despite the actual clang being identical. Using `string:` makes ccache check only change when Xcode/clang actually updates.
4. **`-j $(sysctl -n hw.logicalcpu)`** (8 on Depot M2) instead of a hardcoded smaller value.

**Do NOT reintroduce** (all tried and removed because they broke things):
- Static ccache key → frozen cache.
- Caching the `build/` directory across runs → fragile for ninja (checkout resets mtimes); combined with a "fix git timestamps" step it caused `ninja: error: manifest 'build.ninja' still dirty after 100 tries` (nondeterministic failures).
- Any "touch files to git commit time" hack.
- `compiler_check=content` → broken by brew updating ccache/hiredis between runs.
- `target_compile_definitions(Telegram PRIVATE ...)` for flags only used by one file → adds the define to all ~1480 files, changing every ccache hash. Use `set_source_files_properties` to scope defines to the specific file.

## Sparkle auto-update

TGPro uses **Sparkle 2** for automatic updates from GitHub Releases. The framework is downloaded in CI, linked via CMake (`DESKTOP_APP_DISABLE_SPARKLE=OFF`), and embedded in the `.app` bundle.

- **Ed25519 keys** (`SPARKLE_ED25519_PUBLIC_KEY`, `SPARKLE_ED25519_PRIVATE_KEY`) are GitHub repo secrets. The public key is baked into `Info.plist` via CMake substitution; the private key signs each DMG in CI.
- **`appcast.xml`** is generated after each build, committed to the repo with `[skip ci]` to avoid build loops.
- The `TDESKTOP_USE_SPARKLE` compile define is scoped to `sparkle_mac.mm` only (via `set_source_files_properties`) to avoid invalidating ccache for all other files.
- Sparkle init happens in `Application::run()` after window creation.

## What recompiles when you edit (practical)

| Edit | Recompiles | Wall time |
|------|-----------|-----------|
| Body of a function inside a `.cpp` | that one `.cpp` | ~11 min (floor) |
| A new `.cpp` file | just it + link | ~11 min |
| A leaf/rarely-included `.h` | every `.cpp` that includes it | a bit more |
| A PCH header (`stdafx.h`, generated `scheme.h`/`palette.h`/`style_basic.h`, or hot `base/`,`ui/`,`data/` headers) | ~all 1480 dependents | ~full rebuild |

ccache granularity is **per translation unit (`.cpp`), not per function**. Linking the final binary, brew install, configure, and DMG packaging always run, so the floor is **~11 min — never instant**, even for a one-line change.

## Working with the build

- Watch a run: `gh run watch <id>` (tolerate transient `HTTP 502`s from the API — re-poll, the run is unaffected).
- Read ccache effectiveness in a run's log: the build step ends with `ccache --show-stats` (look at `Hits: N / M`).
- The **first build after any change to a compile flag or a PCH-included header is a warm-up** (it invalidates the cache); the payoff is the next build. To prove caching health, run the same commit twice — hits should be ~99% on the second.
- If hit rate ever regresses: the diagnosis method that found the PCH bug was — build the same commit twice and compare `sha256` of the generated headers and the compiled `*.pch` binaries; whichever differs across runs is the nondeterminism source.

## Local macOS build (rarely used)

The dev box is Linux; real Mac builds follow upstream `docs/building-mac.md` (brew deps + `cmake`). CI is the primary and expected path — prefer it.

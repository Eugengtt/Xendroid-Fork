# Game-specific settings (per-game configs)

Some titles need configuration overrides to run correctly. Keep these out of the
global config — most of them cost performance or change behavior that other
titles don't want — and use the emulator's per-game config mechanism instead.

## How per-game configs work

- Path: `<storage_root>/config/<TITLEID>.config.toml`
  (on Android: `/sdcard/Android/data/<package>/files/xendroid/config/`)
- Loaded automatically when a title launches, *after* the global
  `xenia-canary.config.toml`, so its values override the global ones
  (`src/xenia/emulator.cc` → `config::LoadGameConfig`,
  `src/xenia/config.cc` → `ReadGameConfig`).
- Keys are looked up as `category.name`, so the section header is **required**.
  A bare `clear_memory_page_state = true` without `[GPU]` is silently ignored.
- The title ID is the 8-hex-digit uppercase ID shown in `xe.log` at launch
  (`Title ID: XXXXXXXX`); it is also the title's folder name under `content/`.

Example (adb):

```sh
adb shell 'mkdir -p /sdcard/Android/data/composee.free/files/compose/config'
adb push 544307D5.config.toml /sdcard/Android/data/composee.free/files/compose/config/
```

## Known title requirements

### Ninja Gaiden 2 — `544307D5`

`config/544307D5.config.toml`:

```toml
[GPU]
clear_memory_page_state = true
depth_float24_convert_in_pixel_shader = true
```

- `clear_memory_page_state = true` — fixes missing/broken **character models**.
  This is the documented workaround for Team Ninja titles (the engine reads
  back GPU-written memory; the page-state refresh makes those writes visible).
  Costs a per-frame page-state pass, so don't enable it globally.
- `depth_float24_convert_in_pixel_shader = true` — prevents **GPU hangs →
  device-loss crashes** on Adreno/kgsl (observed on Adreno 830 + Turnip:
  `kgsl ... Fault id:2`, "gpu fault threshold exceeded" in dmesg, then
  `VK_ERROR_DEVICE_LOST` at the swap submission). The default float24↔float32
  host-depth transfer machinery faults the GPU; converting depth in the draw
  pixel shaders bypasses it. Verified 2026-06-12: with both flags NG2 is fully
  playable (the separate job-system freeze was a code fix — FIFO semaphore,
  commit 5b4113562).

### Fable II — `4D5307F1`

No special configuration needed (as of 2026-06-13). The title previously
black-screened at boot and then crashed at several later stages — all of it
was emulator bugs, fixed in the six-commit series ending at the
`movi(VReg2D)` JIT fix: lost `NtReadFile` completion APCs on POSIX hosts
(VFS manifest read never finished), a pause-path deadlock that swallowed
guest crash dumps, a physical-allocator alignment check testing the wrong
address space (every 32KB-aligned E0 allocation failed), a thread-exit
lifetime race (FORTIFY abort on the game's short-lived bank loader threads),
and an a64 JIT vector-constant encoding bug (uncaught Xbyak error on select
masks). Each was general — no Fable-specific hacks shipped.

**Recommended global setting**: `mount_cache = true` (the game can use the
cache partition for its streaming installer; upstream desktop default since
2024-08, Android fork defaults `false`). Also keep `gpu_allow_invalid_fetch_constants = true`.

**Per-game** `config/4D5307F1.config.toml` (best-known as of 2026-06-13):

```toml
[Vulkan]
vulkan_depth_unorm24 = false
```

> **Section header must match the cvar's CATEGORY, not always `[GPU]`.** Per-game
> keys are looked up as `category.name` (`config.cc` `ReadGameConfig`).
> `vulkan_depth_unorm24` is category `Vulkan` → `[Vulkan]`; `clear_memory_page_state`
> etc. are `GPU` → `[GPU]`. A wrong section silently no-ops (the symptom: the
> per-game value is ignored and the global default stands).

- `vulkan_depth_unorm24 = false` forces the float32 depth-emulation path
  instead of native `D24_UNORM_S8_UINT`. This gives the best overall result so
  far on Adreno 830 + Turnip (other geometry/stability improved). It's kept
  per-game on purpose — forcing it globally risks the known float32-depth fault
  on other titles. (This cvar is read at GPU init; it was made per-game-applicable
  by reading it live in `VulkanRenderTargetCache::depth_unorm24_vulkan_format_supported()`
  — the per-game config loads before any guest depth render target is created.)

**Required title patch** `patches/4D5307F1.patch.toml` (ships in the repo;
deploy to `<storage_root>/patches/`):

- Fixes an **intermittent crash during disc streaming** (loading screens). A
  lock-free streaming-deserializer pool checks emptiness via its `count` word
  (`obj+0x138`) instead of the list `head` (`obj+0x16C`); under the fork's true
  8-core concurrency the two desync (`count!=0`, `head==NULL`), the guard
  passes, and `0x82C9F12C` dereferences a null head. The patch changes that one
  guard load to read `head` so the engine's already-handled "pool empty / retry"
  path runs instead. (Reverse-engineered from the guest dump; root cause is the
  360's coarser scheduling + stronger store ordering masking the race.)
- **Patch-system requirements** (so it actually loads): filename must match
  `^[A-Fa-f0-9]{8}.*\.patch\.toml$` — i.e. `4D5307F1.patch.toml` (a bare
  `.patch` extension is silently rejected, "incorrect filename", 0 titles
  loaded). It also needs the module hash (`4145F96D2DEE2AB5`) to match the one
  in the `.toml`. `apply_patches` defaults to `true`. Confirm via the log:
  `PatchDB: Loaded patches for 1 titles` + `Patcher: Applying patch for: Fable II(4D5307F1)`.

**Known remaining issues** (as of 2026-06-13):
- **Missing ground/floor geometry** (UNRESOLVED): the terrain renders see-through
  on the Vulkan backend. Confirmed *not* occlusion/readback (draws are issued,
  ~3.4M, but the ground writes nothing). The `nolrz` experiment proved the
  **precise per-fragment depth test** is the culprit (disabling Turnip LRZ made
  *more* geometry vanish, i.e. LRZ was masking part of the bug) — a reversed-Z +
  float24-precision issue. No documented Vulkan ground fix exists anywhere (even
  the dedicated just-harry femtofork: "ground transparent on Vulkan regardless").
  Do NOT bother with: `readback_resolve`, `readback_memexport`, `occlusion_query`,
  `depth_float24_round`, `render_target_path_vulkan=fsi` (Turnip lacks the feature
  → silent fallback), or `turnip_debug=nolrz` (makes it worse). The remaining
  lead is a code fix: quantize the color-pass shader depth to guest 20e4 float24.
- **Intermittent loading-screen crash** during heavy disc streaming
  (`XFileSectorInformation` flood): guest null-object derefs (atomic spinlock /
  type-dispatch on a near-null object) across engine threads. Pre-existing,
  not driver-related. A guest call-stack was added to the crash dump
  (`emulator.cc`) to identify the null source on the next occurrence.
- Dialogue audio cutoffs (matches upstream compat #74).

> **Caveat: `mount_*` cvars cannot be set per-game.** Cache/scratch/MU devices
> are registered during emulator setup (`xendroid_emu.cpp`, right after
> `Emulator::Setup`), which happens *before* the title launch loads
> `config/<TITLEID>.config.toml` — a per-game override is read too late.

## Device/driver-level notes (global config, not per-game)

These belong in the device's global config because they are properties of the
GPU/driver, not of a title:

- `vulkan_mid_frame_submission_draws` — split long frames into multiple queue
  submissions every N draws. On kgsl-based Adreno devices this prevents
  GPU-idle bubbles on heavy frames (e.g. `1300` on AYN Odin3 / Adreno 830
  locks Forza titles from 20 to 30 fps). `0` (default) disables splitting.
- `depth_float24_convert_in_pixel_shader = true` can also be set globally on
  devices whose driver faults in the float32-depth transfer path regardless of
  title (seen on Retroid Pocket 5 / Adreno 650).
- `vulkan_dynamic_constant_buffers` must stay `false` (the default) on
  Qualcomm proprietary drivers — pixel shaders read zeros through
  dynamic-offset uniform descriptors (a650/a740/a830 blobs). Turnip handles it
  and may opt in for the small binding-update win.

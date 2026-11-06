# Uncharted PC Port — Comprehensive Issue, Bug & Polish Audit

*Generated November 2026 for Uncharted: The Nathan Drake Collection PC Port (v0.8.3).*  
*Use this checklist to triage, prioritize, and track upcoming fixes.*

---

## Table of Contents
1. [Critical & Crash Risks](#1-critical--crash-risks)
2. [Graphics, Rendering & Visual Bugs](#2-graphics-rendering--visual-bugs)
3. [Performance, Stutter & Frame Pacing](#3-performance-stutter--frame-pacing)
4. [Audio & Sound Processing](#4-audio--sound-processing)
5. [Input, Camera & Controls](#5-input-camera--controls)
6. [Launcher & UI Polish](#6-launcher--ui-polish)
7. [Multiplayer (UC2MP) Mod Quirks](#7-multiplayer-uc2mp-mod-quirks)
8. [Code Architecture & Technical Debt](#8-code-architecture--technical-debt)

---

## 1. Critical & Crash Risks

| ID | Issue | Affected | Severity | Status / Fix Details |
|---|---|---|---|---|
| **CRIT-01** | **U1 4K Animation Assert Heap Overflow** | U1 | **High** | **FIXED**: Hardened resolution guards in `apply_preset()` and `change_resolution()` to prevent U1 from ever selecting native 3840x2160 render heap; U1 automatically defaults to native 2880x1620 and upscales cleanly with FSR 1, preserving the 2 GiB animation heap boundary. |
| **CRIT-02** | **U3 Async Readback Memory Faults** | U3 | **High** | **MITIGATED & GUARDED**: Protected readback mode (`GOW3_ASYNC_READBACK=1`) is enforced by default in `probe_uncharted.py` with `BufferCache::noisy_pages` tracking, eliminating invalid compute ring pointer faults. |
| **CRIT-03** | **Windows Red Zone Exception Corruption** | All | **Medium** | **GUARDED**: Red zone leaf function patching (`runtime_redzone_install`) and post-fault context recovery (`runtime_redzone_after_fault`) in `probe.c` are active on all guest ELF text segments. |
| **CRIT-04** | **Stuck Page Nested Fault Freeze** | U3 | **Medium** | **FIXED**: Added a 16-repeat consecutive fault failsafe in `GuestFaultSignalHandler()` (`page_manager.cpp`) that triggers `ReleaseStuckPage()` if an unmonitored page repeatedly traps, breaking infinite nested fault loops and preventing deadlocks. |
| **CRIT-05** | **Garbage T# / Texture Descriptor Crashes** | U2 / U3 | **Medium** | **GUARDED**: `PlausibleImage` and `PlausibleTarget` validate guest sizes, layers, and tiling dimensions in `vk_rasterizer.cpp`, dropping invalid texture/depth descriptors before Vulkan view creation. |

---

## 2. Graphics, Rendering & Visual Bugs

| ID | Issue | Affected | Severity | Details & Fix Path |
|---|---|---|---|---|
| **VIS-01** | **U1 Weapon & Character Flickering with Multithreading** | U1 | **High** | When multithreaded rendering is enabled in Drake's Fortune, weapons and character models intermittently render as flat grey textured models due to a race between command recording and constant buffer updates. *Fix:* Synchronize uniform buffer descriptor sets before worker recording threads commit command buffers. |
| **VIS-02** | **Black Bullet-Hit Circles (Decal Blend Bug)** | U1 | **Medium** | Gunfire bullet impacts on walls and terrain spawn black circular discs instead of alpha-blended dust/debris impact decals. *Fix:* Correct blending state / depth bias for decal rasterization passes and patch the uninitialized alpha component in the decal pixel shader. |
| **VIS-03** | **Smoke Particle Quad Blobs** | U1 | **Medium** | Explosions and grenade smoke occasionally render as solid textured geometric quads rather than translucent volumetric particle sprites. *Fix:* Investigate particle blend mode in `vk_rasterizer.cpp` (depth-test stencil write mask during particle pass). |
| **VIS-04** | **Character Hovering / Sinking on Queued Frames** | U2 / U3 | **Medium** | Setting `frames_queued` > 1 in the launcher causes Nate and NPCs to hover slightly above ground or have their feet sink into floors during locomotion. Caused by delayed 64-wide copy shader readbacks (`cs 6a6422a9`). *Fix:* Synchronize actor bone transform readback fences directly at frame end-of-pipe. |
| **VIS-05** | **Treasure Model Depth Clipping with Upscalers** | U2 / U3 | **Low** | In the Treasures inspection viewer, 3D artifact models can render invisible or clip against the black background when temporal upscalers (FSR / DLSS) are active. *Fix:* Ensure `TemporalUpscaler::RedirectDepth` properly blits UI depth buffers before rendering the inspect composite pass. |
| **VIS-06** | **Rare Grey World on Level Streaming (Temple 0a/0b)** | U1 | **Low** | Rarely after skipping cutscenes into temple-0a/0b, terrain and sky geometry render pure grey while Nate renders black, until a treasure is picked up or a checkpoint reloads. *Fix:* Force-refresh dirty lighting table records and unflattened light buffers on level streaming pack boundaries. |
| **VIS-07** | **AMD RDNA Vertex Motion Buffer Offset Fault** | All | **Low** | On AMD GPUs with FSR 3.1 enabled, 64-bit vertex motion calculations previously caused `ReadInvalid` device lost faults due to 32-bit truncation. Addressed with carry-add, but needs verification on older RDNA1 and Vega architectures. *Fix:* Add fallback 32-bit structured buffer indexing for pre-RDNA2 AMD drivers. |

---

## 3. Performance, Stutter & Frame Pacing

| ID | Issue | Affected | Severity | Details & Fix Path |
|---|---|---|---|---|
| **PERF-01** | **On-Demand Shader Compilation Hitching** | All | **High** | Entering a newly encountered area or triggering a novel particle/lighting compute pass causes 15–35 ms frame-time spikes while shaders compile. *Fix:* Implement comprehensive pipeline pre-warming during chapter loading screens and expand the async worker compiler thread pool. |
| **PERF-02** | **VRAM Accumulation & Late Garbage Collection** | All | **High** | VRAM usage steadily grows up to 5.6+ GB in long sessions before the image garbage collector purges stale textures, which can exhaust memory on 4 GB–6 GB cards (GTX 1650, RTX 3050). *Fix:* Lower texture cache eviction threshold from 80% to 65% on GPUs with <= 6 GB detected VRAM. |
| **PERF-03** | **Shader Cache Crowding / Compile Storm Leftovers** | U2 / U3 | **Medium** | Pre-0.8.0 shader caches contain thousands of duplicate compute pipeline keys (e.g. shader `8f067540` having 1,000+ versions), increasing warm-up time by up to 300 seconds on first launch. *Fix:* Automatically purge legacy duplicate hashes from `user/cache/` on launcher startup. |
| **PERF-04** | **CPU Draw Call Bottleneck on Laptop CPUs** | U3 | **Medium** | On mobile CPUs (Intel Core i5/i7 H/U series, AMD Ryzen mobile), U3 hits a ~48–54 FPS ceiling due to single command thread bottleneck (~54,000 draws/second). *Fix:* Enhance Vulkan secondary command buffer recording in `GOW3_VK_RECORD_THREAD` to balance worker thread dispatch. |
| **PERF-05** | **Micro-Stutter at 120 Hz / 144 Hz Presentation** | All | **Medium** | When playing with unlocked frame rates on high refresh rate displays, variable vblank timing can cause micro-jitter during fast camera panning. *Fix:* Decouple camera simulation ticks from presentation swap intervals; implement precise sub-millisecond frame pacing. |
| **PERF-06** | **External USB / exFAT Drive I/O Stalls** | All | **Low** | Running the game from slow external hard drives or USB flash drives formatted as exFAT causes 500–800 ms freeze spikes during background file chunk streaming. *Fix:* Implement asynchronous non-blocking file streaming and increase file cache read-ahead buffers. |

---

## 4. Audio & Sound Processing

| ID | Issue | Affected | Severity | Details & Fix Path |
|---|---|---|---|---|
| **AUD-01** | **Audio Buffer Queue Latency (~80 ms)** | All | **Medium** | **FIXED**: Lowered WASAPI device sample frames from 480 to 256 and capped queue periods to 1 in `runtime_audio.c` and `probe_uncharted.py`, dropping output latency from ~80 ms to a snappy ~25–30 ms. |
| **AUD-02** | **Stereo Downmix Balance on Dialogue** | U2 / U3 | **Medium** | **FIXED**: Increased the center vocal channel fold-in multiplier to `0.841` (+1.5 dB) in `downmix_stereo()`, ensuring clear, audible speech over loud surround action effects in headphones and stereo speakers. |
| **AUD-03** | **Rapid Firefight Sound Starvation** | U1 / U2 | **Low** | Under extreme continuous weapon fire (e.g. grenade launchers and heavy machine guns firing simultaneously), ATRAC9 voice decoding occasionally skips audio frames. *Fix:* Enlarge the ATRAC9 decode packet ring buffer in `runtime_ajm.c`. |

---

## 5. Input, Camera & Controls

| ID | Issue | Affected | Severity | Details & Fix Path |
|---|---|---|---|---|
| **INP-01** | **Mouse Look Sensitivity Discrepancy** | All | **Medium** | The mouse sensitivity multiplier (0.5x–2.0x) behaves non-linearly across different Windows desktop pointer speed settings and high-DPI gaming mice (1600+ DPI). *Fix:* Use raw Windows `WM_INPUT` coordinates rather than standard cursor deltas, and add a finer sensitivity slider (0.1x to 3.0x). |
| **INP-02** | **PlayStation Touchpad Button on Xbox Controllers** | All | **Low** | Prompts for opening journal or weapon select show the PlayStation touchpad glyph even when playing on an Xbox controller. *Fix:* Map the touchpad click to the Xbox `View` / `Back` button and display the corresponding Xbox icon dynamically when an XInput controller is active. |
| **INP-03** | **Mouse Look in U1 Cover Camera** | U1 | **Low** | **FIXED**: Enabled `GOW3_MOUSE_LOOK_U1_COVER=1` by default in `probe_uncharted.py`, providing smooth 1:1 mouse aiming from cover in Drake's Fortune. |

---

## 6. Launcher & UI Polish

| ID | Issue | Affected | Severity | Status / Fix Details |
|---|---|---|---|---|
| **UI-01** | **Presets Tab Settings Synchronization** | Launcher | **Medium** | **FIXED**: `select_game()` and `on_preset_game_change()` now invoke `redraw_presets()` immediately upon game selection, refreshing active preset cards and resolution indicators across the grid without requiring setting toggles. |
| **UI-02** | **Secondary Display Multi-Monitor Opening** | Launcher | **Low** | **FIXED**: `center_window()` now queries the cursor's current monitor via `MonitorFromPoint` and `GetMonitorInfoW`, centering properly on the active display's usable work area before falling back to the primary display. |
| **UI-03** | **Game Selection Card Covers & Title Casing** | Launcher | **Low** | **FIXED**: Added high-definition portrait box art thumbnails (`1.jpg`, `2.jpg`, `3.jpg`) with anti-aliased rounded corners and subtle dark borders to the game selector cards; converted all card titles and taglines from ALL CAPS to clean Title Case. |
| **UI-04** | **Live In-Game Overlay Settings Delay** | Launcher | **Low** | Toggling performance overlay metrics (FPS, GPU, VRAM) in the launcher's Display tab does not update an already-running game session immediately. *Fix:* Write live overlay toggle triggers directly to `user/overlay.trigger` so changes apply on the next rendered frame. |
| **UI-05** | **Window Resize Border Glitches** | Launcher | **Low** | Fast manual resizing of the launcher window can cause temporary flickering or blank canvas backgrounds before Tkinter redraws children. *Fix:* Bind `<Configure>` events with a 30 ms debounce timer for smooth canvas resizing. |

---

## 7. Multiplayer (UC2MP) Mod Quirks

| ID | Issue | Affected | Severity | Details & Fix Path |
|---|---|---|---|---|
| **NET-01** | **Lobby Search UI Blocking on Bad Network IP** | UC2MP | **Medium** | If a player searches for a net game and the specified host IP/port is unreachable, socket polling can block the Tkinter launcher event loop for up to 3 seconds. *Fix:* Run LAN search broadcasts asynchronously inside a background worker thread. |
| **NET-02** | **Remote Avatar Fallback to Default Skin** | UC2MP | **Low** | When a remote player joins with an unextracted or DLC multiplayer character skin, the game can fail to load the model mesh and fall back to Nathan Drake's default hero model. *Fix:* Add a graceful model fall-back table in `runtime_uc2mp_catalog.h`. |
| **NET-03** | **Mid-Match Joining State Synchronization** | UC2MP | **Low** | Joining a session while a map transition is in progress can cause the client to drop the initial level packet, resulting in an infinite loading screen. *Fix:* Implement match phase handshake checks in `runtime_uc2mp_netmaps.c`. |

---

## 8. Code Architecture & Technical Debt

| ID | Issue | Affected | Severity | Details & Fix Path |
|---|---|---|---|---|
| **ARCH-01** | **Legacy "gow3" Naming Throughout Codebase** | All | **Low** | File names (`gow3gpu.cpp`, `gow3_trophies.cpp`), symbols, and environment variables (`GOW3_*`) still carry legacy naming from the original runtime codebase. *Fix:* Gradually introduce aliased `UNCHARTED_*` environment variables and namespace wrappers. |
| **ARCH-02** | **Hardcoded Guest Memory Base Addresses** | All | **Low** | Many shaders and runtime memory lookups use hardcoded hex literals (`0x1000000000`, `0x1100000000`) rather than named memory segment constants. *Fix:* Centralize all guest virtual address mappings into a single `guest_layout.h` header. |
| **ARCH-03** | **Direct Memory Access (DMA) Pointer Sync Overhead** | All | **Low** | The CPU/GPU memory synchronization layer verifies hundreds of pointers per frame in `GOW3_DMA_SYNC`, consuming ~75 ms per 5 seconds of CPU time. *Fix:* Implement dirty-page tracking so only modified memory regions undergo pointer reconciliation. |

---

## Triage Priority Recommendation

1. **Immediate Focus (Stability & Stutter)**:
   - `PERF-01`: Shader pre-warming and async compilation to eliminate area-transition stutters.
   - `PERF-02`: Dynamic VRAM eviction threshold for 4–6 GB GPUs.
   - `AUD-01`: Reduce audio buffer latency to ~30 ms for immediate gunshot/action response.
2. **Next Pass (Visuals & Polish)**:
   - `VIS-02` & `VIS-03`: Fix U1 black bullet-hit circles and smoke particle quad rendering.
   - `UI-01`: Auto-sync Presets tab game selection cards.
   - `INP-01`: Raw mouse input polling without Windows pointer scale curves.


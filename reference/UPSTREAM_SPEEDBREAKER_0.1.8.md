# SpeedBreaker v0.1.8: what ReCarbon can reuse

Research and planning only (2026-10-23). This changes no milestone scope, criterion or code.

- **Upstream range:** v0.1.7 (`31849f5`, ReCarbon's base) to v0.1.8 (`86cfaa4`), one squashed
  release commit: 60 files, +11,078/−208, read in the local read-only `../NFS/speedbreaker`
  checkout.
- **"Verified upstream"** means SpeedBreaker's own tests or measurements on NFS: Most Wanted.
  Nothing here is verified for Carbon until ReCarbon reproduces it.
- **Addresses:** MW addresses (`sub_8…`) never apply to Carbon as they are (AGENTS.md).

## Changes in v0.1.8

| # | Area | What and why (upstream) | Kind | Applies to ReCarbon? | ReCarbon status | Suggested action |
|---|---|---|---|---|---|---|
| 1 | Audio reliability | **EA SND CPU budget** (`apu/ea_cpu_budget.*`, issue #35). EA's sound library times its mixer thread with `mftb`; a host thread the OS doesn't run for ~14 ms counts as an overloaded mix, and EA expels voices (the engine drops to a murmur until the next gear change). Fix: hook the expel step to do nothing (unless `NFSMW_EA_CPU_BUDGET=1`), keep the check, log what would have been dropped. **Verified upstream:** `ea_cpu_budget_test` (EA's rule as a model, plus asserting the generated code still matches the hooked functions); measured in 3 of 4 sandbox drag races on a MacBook. | Bug fix (title-specific addresses, generic idea) | **Maybe.** Carbon is EA too (its sound thread is `RWAudioCore Dac`), but whether it has this budget rule is a **hypothesis**. MW's addresses don't apply. | Not present. The QoS fix (`20dd2ed`) runs XAudio's mixer at user-interactive, which reduces the trigger (mixer starvation) but not the rule. | Look for Carbon's equivalent: an `mftb`-timed mixer, a three-block average and a voice-expel loop. Confirm the symptom in a long race (sound dropping until re-triggered). Port with Carbon addresses and a test like upstream's. |
| 2 | Memory safety | **Releasing pages on 16 KB hosts** (`kernel/vmem.cpp` `ResetHostRange`). `mmap(MAP_FIXED)` rounds its length up to whole host pages, so returning a 4 KB-page guest range that ends inside a 16 KB host page zeroed the guest pages after it, possibly another allocation's. Fix: remap only the whole host pages inside the range and clear the head and tail in place. **Verified upstream:** `vmem_host_pages_test` (injected host page size, a model kernel that rounds and zeroes like the real one, plus the real `mmap`). Not seen in play upstream: preventive. | Bug fix, game-agnostic | **Yes.** Same code. | **Has the bug:** `runtime/kernel/vmem.cpp:21`, the v0.1.7 remap. | Port the fix and its test. It's generic and needs no Carbon address. |
| 3 | Memory | `write_watch` sub-page tracking defaults to on wherever host pages are over 4 KB (was "on Apple"). | Runtime improvement, generic | Only off Apple | macOS already defaults to on | None for macOS; take it with a future sync. |
| 4 | Frame pacing (optional) | **Smooth Motion / 120 FPS** (`game/hfr*`, `video/hfr_schedule.h`, `VblankLock::PresentAt(vblank, between)`, `HoldMultiple`). The game steps at 60 and draws an in-between picture. Its core reads MW's world stepper and pose history. | Feature, mostly MW-specific | **Optional future feature** | Not present. M7 frame pacing uses host-present timing (decision). | Out of scope for alpha. Reconsider after M7 with Carbon's own stepper. |
| 5 | Frame pacing / GPU | **Vertex-range narrowing** (shader translator `vertexStride/vertexExtent/vertexIndexOther`; renderer guards only the draw's vertex records of a dynamic buffer). It keeps the rest of the buffer writable, which matters for HFR's extra draws. | Runtime improvement, generic | Maybe (performance) | Not present | Measure first: only if dynamic-buffer guard stalls show in Carbon's races (M7 profiling). |
| 6 | Rendering | **Fades short of the screen's edges** on screens taller than 16:9 (`gpu/edge_bars.h` `PlanLayer`, MW's front-end matrix). Plus a **covers diagnostic** (`gpu/covers.h`, `NFSMW_LOG_COVERS`) that logs dark boxes and layers that stop short of the picture. | Bug fix (MW-specific) + diagnostic technique | No for the fix: ReCarbon shows 16:9 letterboxed, with no Vert+/edge bars. The diagnostic: later. | n/a | Reuse the covers idea only if widescreen hooks come back (M8 optional). |
| 7 | Input (excluded) | Two-player **split screen** (`hid/player_slots.h`, `game/split_screen.*`, `gpu/split_views.h`) | Feature | **Excluded from planning** | n/a | None |
| 8 | Input (optional) | **USB steering wheels** (`hid/wheel*`, Logitech G29, force feedback through SDL) | Feature | **Optional future feature** | n/a | After M8, if wanted |
| 9 | Testing | **World-step timed input** (`RECARBON_INPUT_SCRIPT`-style `w<step>` taps and holds, `…_STEPS_AFTER`): presses land on the same simulation step however long loads take. Uses MW's stepper address. | Testing technique | **Yes (idea).** Needs Carbon's step counter (the M6 frame-time hook). | Blind wall-clock timelines in M6 kept missing prompts (WORKLOG). | Once Carbon's world stepper is found, add step-timed taps for deterministic routes. |
| 10 | Testing | **Virtual-pad hotplug** steps (`DETACH`/`ATTACH`) and several virtual pads. | Testing technique | Yes (hotplug only; multi-pad is split screen) | ReCarbon has one virtual pad, no hotplug steps | Add `DETACH`/`ATTACH` for M5/M7 controller hot-unplug tests. |
| 11 | Testing | **Tests that assert the generated code** still matches what a hook names (`ea_cpu_budget_test <ppc>`). Another XEX or config then fails in a test, not silently in the game. | Testing pattern | Yes | ReCarbon's `validate_recomp.py` hook check (in progress) does the generic half | Use it for every future title hook. |

`SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS` for virtual pads moved upstream but isn't new:
ReCarbon already has it (`hid/input.cpp:287`). The ImGui `HasGamepad` change is for wheels only.

## Relation to ReCarbon's recent fixes
- **`XRTL_CRITICAL_SECTION` endianness (`698dd9d`):** upstream v0.1.8 doesn't change it.
  ReCarbon's patch 0010 is ahead of upstream. Worth reporting upstream if MW could hit it.
- **Thread priority → QoS (`20dd2ed`):** not in v0.1.8. It's complementary to #1: the QoS
  mapping reduces mixer starvation, and #1 removes EA's reaction to whatever starvation
  remains.
- **`getGradients` (`82c3026`):** not in v0.1.8. MW doesn't use it; ReCarbon is ahead.
- **Render-queue fences:** in progress in ReCarbon, the same pattern as SpeedBreaker's MW
  hooks. v0.1.8 doesn't change them.

## Recommendations (value / risk / effort)

| Priority | Item | Value | Risk | Effort | When |
|---|---|---|---|---|---|
| 1 | Port the 16 KB-page release fix (#2) with its test | High (latent memory corruption, generic) | Low | Low | Next, any milestone (a bug fix) |
| 2 | Investigate Carbon's EA SND CPU budget (#1): find it, confirm, port | High (audio reliability in races) | Medium (title hooks) | Medium | M7, with racing audio; sooner if sounds drop |
| 3 | Virtual-pad `DETACH`/`ATTACH` (#10) | Medium (controller tests without a person) | Low | Low | With the M5 controller hand test |
| 4 | World-step timed input (#9), after Carbon's stepper is found | Medium (deterministic automation) | Low | Medium | M6–M7, with the frame-time hook |
| 5 | Generated-code assertions for title hooks (#11) | Medium | Low | Low | With each new hook |
| 6 | Vertex-range narrowing (#5) | Low–Medium (performance) | Medium | Medium | Only if M7 profiling shows guard stalls |
| — | Smooth Motion / 120 FPS (#4), steering wheels (#8) | Optional | — | High | Optional, post-alpha |
| — | Split screen and multiple controllers (#7) | Excluded | — | — | Not planned |

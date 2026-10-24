# Proposal: test changes ReCarbon could offer SpeedBreaker

A read-only proposal (2026-10-24). Nothing here has been sent to SpeedBreaker or changed in
`../speedbreaker`; any contribution waits for the user's approval. Each claim about upstream
was checked against its v0.1.8 sources. A claim that a test misses a bug was checked by
building upstream's own test and runtime into a scratch folder with the bug planted.

## Verified upstream gaps

1. **`tests/run_instr_tests.sh` can pass with tests missing.** Three things go unnoticed:
   - XenonRecomp's test mode leaves out any test function with an instruction it can't
     translate, and the script doesn't fail.
   - When the test build fails, the script goes on to run the previous binary.
   - The exit status doesn't reflect failed checks.

   ReCarbon's copy fails in all three cases. *Offer:* the three hunks (ReCarbon's
   `tests/run_instr_tests.sh`).
2. **Guest system time can step back.** `runtime/cpu/guest_time.h:72` reads `system_clock`
   on every call. It has 1 µs resolution, and steps back when threads read it close together,
   or by milliseconds to seconds when the host's time is adjusted. ReCarbon hit this in M4.
   *Offer:* the fix with `tests/system_time_monotonic_test.cpp`.
3. **frame_rate_policy_test misses two rules of Auto.** Upstream's test passes with either of
   these planted:
   - the window not trimmed to its last 10 s;
   - the window kept across a switch.

   *Offer:* ReCarbon's cases `WindowIsRecent` and `NoEvidenceAcrossSwitch`.
4. **swapchain_policy_test misses the settle watch's triggers.** Upstream's test passes with
   either of these removed:
   - a size event starting the watch;
   - a new swapchain starting the watch.

   *Offer:* `EachStartsTheWatch`.
5. **report_zip_test's C++ part misses Adler-32 past its modulus.** With `a %= 65536` planted,
   it passes. Upstream's Python reader would catch it, but that reader isn't a CTest.
   *Offer:* one 100,000-byte check against zlib's values.
6. **swapchain_use_test's work guard depends on the scheduler.** It requires more than 1000
   waiter uses in one run. On a MacBook Air M5 it failed 1 run in 50 alone, and one full
   ctest run (539 uses).
   *Offer:* run the work guard, and the v0.1.5 canary, until their conditions are met, for up
   to 10 s, with every run still checked for soundness. With it, 10 of 10 full `ctest -j8` runs
   passed under an 8-process CPU load (load average up to 66); counting over 5 runs alone had
   failed most full runs at a load average of 13–19.
7. **No translation of getGradients** (texture fetch opcode 18): upstream's
   `gpu/shader_translator.cpp` has none. Carbon's road shader needs it; it is unknown
   whether MW uses it.
   *Offer:* the translation with `tests/shader_gradients_test.cpp`.

## Offers that need upstream's judgement

- **Instructions:** XenonRecomp patch 0009 (`eqv`, `dcbst`, `bdnzt`), with 15 hand-computed
  tests (`tests/instr/carbon_*.s`). Unused by MW, as far as is known.
- **Guest critical sections:** patch 0010 (big-endian `XRTL_CRITICAL_SECTION` fields), with
  `tests/critical_section_test.cpp`. It is generic, but whether MW reaches it wasn't checked.
- **Tests that need their runtime fixes contributed with them:** `thread_priority_test`,
  `ring_state_test`, `raise_exception_test` and `edram_format_test`. Their headers don't
  exist upstream.
- **The installer check's process cleanup:** process group, descendant snapshot with start
  times, and owner marker (`scripts/install_check.sh`). Upstream has no equivalent script.
  Three macOS details on the way are generally useful:
  - `ps -e` is every process; environments need `-E`.
  - `ps -E` shows no environment for Apple's own binaries.
  - A copied `/bin/sleep` is killed at launch.
- **Gaps in both projects:**
  - present_schedule's 120 Hz band edges (117–123 Hz) are untested;
  - `renderer.cpp`'s scaled-pitch window and Auto's budget choice can't be tested without
    moving them into a header.

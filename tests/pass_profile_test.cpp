// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// gpu/pass_profile.h: RECARBON_PASS_PROFILE_SPLIT=<draws> (a diagnostic, off by default; only with
// RECARBON_PASS_PROFILE) ends a profiled frame's render pass every <draws> draws, so each slice of
// the frame gets its own GPU time, and lists every draw of a slice as runs of the same shader
// pair. Without it the profile is as before: whole passes, their first 3 draws named. Header only.
// From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/pass_profile_test.cpp -o build/pass_profile_test && build/pass_profile_test
#include <gpu/pass_profile.h>

#include <cstdio>
#include <string>

namespace
{
    int g_failures = 0;

    void Check(bool ok, const char* what)
    {
        if (!ok)
        {
            printf("  FAIL: %s\n", what);
            g_failures++;
        }
    }

    using gpu::passprofile::DrawRuns;
    using gpu::passprofile::kMaxRuns;
    using gpu::passprofile::kMaxSplit;
    using gpu::passprofile::ParseSplit;
    using gpu::passprofile::SplitDue;
}

int main()
{
    printf("---- RECARBON_PASS_PROFILE_SPLIT ----\n");
    Check(ParseSplit(nullptr) == 0, "unset: off");
    Check(ParseSplit("") == 0, "empty: off");
    Check(ParseSplit("0") == 0, "0: off");
    Check(ParseSplit("1") == 1, "1: every draw its own pass");
    Check(ParseSplit("16") == 16, "16");
    Check(ParseSplit("128") == kMaxSplit && kMaxSplit == 128, "128, the most");
    Check(ParseSplit("129") == 0, "129: off (no larger than the renderer's own 128-draw check)");
    Check(ParseSplit("99999999999") == 0, "too large to parse: off");
    Check(ParseSplit("-3") == 0, "negative: off");
    Check(ParseSplit("8x") == 0, "trailing junk: off");
    Check(ParseSplit("1a") == 0 && ParseSplit("2:") == 0 && ParseSplit("1.5") == 0, "a digit then junk that stays under 128: off");
    Check(ParseSplit(" 8") == 0, "leading space: off");
    Check(ParseSplit("abc") == 0, "not a number: off");

    printf("---- when a pass ends ----\n");
    Check(!SplitDue(0, true, 1000), "off: never");
    Check(!SplitDue(16, false, 1000), "not a profiled frame: never");
    Check(!SplitDue(16, true, 15), "15 draws of 16: not yet");
    Check(SplitDue(16, true, 16), "16 of 16: before the 17th");
    Check(SplitDue(16, true, 40), "past it (a pass the renderer kept open): end it");
    Check(SplitDue(1, true, 1), "1: before every draw but a pass's first");
    Check(!SplitDue(1, true, 0), "a pass with no draws yet: never");

    printf("---- the draws a slice lists ----\n");
    {
        DrawRuns runs;
        Check(runs.Describe().empty(), "no draws: nothing");
        runs.Add(0xA, 0xB, 36);
        runs.Add(0xA, 0xB, 120);
        runs.Add(0xC, 0xD, 528);
        runs.Add(0xA, 0xB, 6);
        Check(runs.Describe() ==
                  " [vs_000000000000000a ps_000000000000000b x2 n156] [vs_000000000000000c ps_000000000000000d x1 n528]"
                  " [vs_000000000000000a ps_000000000000000b x1 n6]",
            "consecutive draws of a pair merge; order and vertex totals kept");
    }
    {
        DrawRuns runs;
        for (uint32_t i = 0; i < kMaxRuns + 5; i++)
            runs.Add(i, i, 3);
        const std::string d = runs.Describe();
        Check(d.find(" +5 more runs (5 draws)") != std::string::npos, "past kMaxRuns runs: a count, not more names");
        Check(d.find(std::string("ps_") + "0000000000000020") == std::string::npos && d.find("ps_000000000000001f") != std::string::npos,
            "the first kMaxRuns runs named");
    }
    {
        DrawRuns runs;
        for (int i = 0; i < 1000; i++)
            runs.Add(1, 2, 3);
        Check(runs.Describe() == " [vs_0000000000000001 ps_0000000000000002 x1000 n3000]", "one long run");
    }

    if (g_failures)
    {
        printf("%d FAILED\n", g_failures);
        return 1;
    }
    printf("all passed\n");
    return 0;
}

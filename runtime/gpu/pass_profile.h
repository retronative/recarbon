// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// RECARBON_PASS_PROFILE_SPLIT=<draws> (a diagnostic, off by default; with RECARBON_PASS_PROFILE
// only): in a profiled frame, the renderer (gpu/renderer.cpp) ends the render pass every <draws>
// draws, so each slice of the frame gets its own GPU time, and lists every draw of each slice as
// runs of the same shader pair. Without it a profiled pass names only its first 3 draws, and
// splitting its time among those misattributed shader cost (measured on a MacBook Air M5, October 2026).
// Each extra pass end and start adds GPU time of its own (measured 30-90 us for a 1-128-draw pass
// at 3x), so a split profile ranks draw ranges; it is not the frame's unsplit cost. Pure
// functions, for tests/pass_profile_test.cpp.
#pragma once
#include <cstdint>
#include <format>
#include <string>
#include <vector>

namespace gpu::passprofile
{
    // No larger than the renderer's own 128-draw submit check, which already ends passes there.
    constexpr uint32_t kMaxSplit = 128;
    // Runs named per slice; past them, a count.
    constexpr uint32_t kMaxRuns = 32;

    // The variable's value: 1-kMaxSplit in plain digits, else 0 (off).
    inline uint32_t ParseSplit(const char* v)
    {
        if (!v || !*v)
            return 0;
        uint32_t n = 0;
        for (const char* p = v; *p; p++)
        {
            if (*p < '0' || *p > '9')
                return 0;
            n = n * 10 + uint32_t(*p - '0');
            if (n > kMaxSplit)
                return 0;
        }
        return n;
    }

    // End the open pass before the next draw: a profiled frame, the split on, and the pass already
    // holding `split` draws or more.
    inline bool SplitDue(uint32_t split, bool profiling, uint32_t drawsInPass)
    {
        return split && profiling && drawsInPass >= split;
    }

    // A slice's draws in order, consecutive draws of one shader pair merged.
    struct DrawRuns
    {
        struct Run { uint64_t vs, ps; uint32_t draws; uint64_t vertices; };
        std::vector<Run> runs;

        void Add(uint64_t vs, uint64_t ps, uint32_t vertices)
        {
            if (!runs.empty() && runs.back().vs == vs && runs.back().ps == ps)
            {
                runs.back().draws++;
                runs.back().vertices += vertices;
                return;
            }
            runs.push_back({ vs, ps, 1, vertices });
        }

        std::string Describe() const
        {
            std::string text;
            uint32_t moreRuns = 0, moreDraws = 0;
            for (size_t i = 0; i < runs.size(); i++)
            {
                const Run& r = runs[i];
                if (i < kMaxRuns)
                    text += std::format(" [vs_{:016x} ps_{:016x} x{} n{}]", r.vs, r.ps, r.draws, r.vertices);
                else
                {
                    moreRuns++;
                    moreDraws += r.draws;
                }
            }
            if (moreRuns)
                text += std::format(" +{} more runs ({} draws)", moreRuns, moreDraws);
            return text;
        }
    };
}

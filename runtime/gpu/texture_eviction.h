// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// Which cached textures the renderer (gpu/renderer.cpp) destroys at the end of
// a frame, so the texture cache stops growing while driving (it kept every
// texture it made until a setting reloaded them: about 500 MB more over 7 laps
// or 20 minutes of free-roam). Only when the textures' bytes are over the
// budget (0: never), and only textures that are:
//   - done with on the GPU: the last submission that used one has completed;
//   - unused for minIdleFrames frames at least, so a lap's working set stays;
//   - not scaled (made from the scaled resolve memory, which keeps its own
//     account of them).
// Stale ones first (their guest memory was written since they were loaded:
// the next use would load them again anyway), then the longest unused, the
// key breaking ties; at most maxPerFrame a frame, so no frame destroys
// hundreds at once; and no more than takes the bytes down to the budget.
// Vulkan-free: tests/texture_eviction_test.cpp checks it alone.
#pragma once
#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace gpu::eviction
{
    struct Candidate
    {
        uint64_t key;               // the texture cache's key
        uint64_t bytes;             // its image's size
        uint64_t lastFrame;         // the frame it was last used in
        uint64_t lastSubmission;    // the submission it was last used in
        bool scaled;
        bool stale;                 // its guest memory was written since it was loaded
    };

    struct Policy
    {
        uint64_t frame;                 // this frame
        uint64_t completedSubmission;   // every submission up to this one has completed
        uint64_t usedBytes;             // all the cached textures' bytes
        uint64_t budgetBytes;           // 0: no eviction
        uint64_t minIdleFrames;
        size_t maxPerFrame;
    };

    // The keys to destroy, in the order chosen.
    inline std::vector<uint64_t> Select(std::span<const Candidate> candidates, const Policy& p)
    {
        std::vector<uint64_t> keys;
        if (p.budgetBytes == 0 || p.usedBytes <= p.budgetBytes)
            return keys;
        std::vector<const Candidate*> eligible;
        for (const Candidate& c : candidates)
            if (!c.scaled && c.lastSubmission <= p.completedSubmission && c.lastFrame <= p.frame
                && p.frame - c.lastFrame >= p.minIdleFrames)
                eligible.push_back(&c);
        std::sort(eligible.begin(), eligible.end(), [](const Candidate* a, const Candidate* b) {
            if (a->stale != b->stale)
                return a->stale;
            if (a->lastFrame != b->lastFrame)
                return a->lastFrame < b->lastFrame;
            return a->key < b->key;
        });
        uint64_t used = p.usedBytes;
        for (const Candidate* c : eligible)
        {
            if (used <= p.budgetBytes || keys.size() >= p.maxPerFrame)
                break;
            keys.push_back(c->key);
            used -= std::min(used, c->bytes);
        }
        return keys;
    }
}

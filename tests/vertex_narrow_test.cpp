// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// Large dynamic vertex ranges guarded or copied as each draw's vertices only (from SpeedBreaker
// v0.2.0's NFSMW_NARROW_VERTICES, renderer.cpp's draw guard loop): the translator's per fetch
// constant stride, extent and "indexes otherwise" marks (gpu/shader_translator.cpp, shaders
// assembled by hand from Xenia's ucode.h encodings: no game microcode), and the range arithmetic
// and dynamic-buffer cache (gpu/vertex_narrow.h).
// From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/vertex_narrow_test.cpp runtime/gpu/shader_translator.cpp runtime/gpu/xenos/ucode.cpp -o build/vertex_narrow_test
#include <gpu/shader_translator.h>
#include <gpu/vertex_narrow.h>

#include <cstdint>
#include <cstdio>
#include <vector>

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

    struct Fetch
    {
        uint32_t src = 0, dst = 1, slot = 0, stride = 8;
        int32_t offset = 0;
        bool mini = false;
    };

    // One exec_end of `fetches.size()` vertex fetches (k_32_32_32_32_FLOAT, dest xyzw).
    std::vector<uint32_t> Shader(const std::vector<Fetch>& fetches)
    {
        constexpr uint32_t kExecEnd = 2;
        uint32_t sequence = 0;
        for (size_t i = 0; i < fetches.size(); i++)
            sequence |= 1u << (2 * i);  // each a fetch
        const uint32_t cfLo = 1u | (uint32_t(fetches.size()) << 12) | (sequence << 16);
        const uint32_t cfHi = kExecEnd << 12;
        std::vector<uint32_t> ucode{ cfLo, cfHi, 0 };
        for (const Fetch& f : fetches)
        {
            const uint32_t d0 = 0u /* vfetch */ | (f.src << 5) | (f.dst << 12) | (1u << 19) | ((f.slot / 3) << 20) | ((f.slot % 3) << 25);
            const uint32_t d1 = (0u | (1u << 3) | (2u << 6) | (3u << 9)) | (38u << 16) | ((f.mini ? 1u : 0u) << 30);
            const uint32_t d2 = (f.stride & 0xFF) | ((uint32_t(f.offset) & 0x7FFFFF) << 8);
            ucode.insert(ucode.end(), { d0, d1, d2 });
        }
        return ucode;
    }

    bool Other(const gpu::TranslatedShader& s, uint32_t slot) { return (s.vertexIndexOther[slot / 32] >> (slot % 32)) & 1; }

    void TestTranslator()
    {
        printf("the translator: stride, extent, and fetches that don't index by the vertex\n");
        {
            auto u = Shader({ { .slot = 0, .stride = 8, .offset = 0 }, { .dst = 2, .slot = 0, .offset = 3, .mini = true } });
            auto s = gpu::TranslateShader(gpu::ShaderStage::Vertex, u.data(), u.size());
            Check(s.ok, "two fetches of r0.x translate");
            Check(s.vertexStride[0] == 8 && s.vertexExtent[0] == 7 && !Other(s, 0), "stride 8 dwords, extent offset 3 + 4");
        }
        {
            auto u = Shader({ { .src = 1, .slot = 3 } });
            auto s = gpu::TranslateShader(gpu::ShaderStage::Vertex, u.data(), u.size());
            Check(s.ok && Other(s, 3) && s.vertexStride[3] == 0, "an index from r1: not the vertex index");
        }
        {
            auto u = Shader({ { .src = 0, .dst = 0, .slot = 0 }, { .src = 0, .dst = 1, .slot = 3 } });
            auto s = gpu::TranslateShader(gpu::ShaderStage::Vertex, u.data(), u.size());
            Check(s.ok && !Other(s, 0) && Other(s, 3), "r0 read after a fetch wrote it: no longer the vertex index");
        }
        {
            auto u = Shader({ { .slot = 6, .offset = -1 } });
            auto s = gpu::TranslateShader(gpu::ShaderStage::Vertex, u.data(), u.size());
            Check(s.ok && Other(s, 6), "a negative offset");
        }
        {
            auto u = Shader({ { .slot = 0 } });
            auto s = gpu::TranslateShader(gpu::ShaderStage::Pixel, u.data(), u.size());
            Check(s.ok && Other(s, 0), "a pixel shader's fetch");
        }
    }

    void TestRange()
    {
        printf("the range: the draw's vertices' records\n");
        using gpu::vertexnarrow::Narrow;
        uint32_t base = 0x1000, size = 0x50000;
        // Vertices 100..199, 32-byte records, each reading 28 bytes: [3200, 199 * 32 + 32).
        Check(Narrow(base, size, 100, 199, 32, 28) && base == 0x1000 + 3200 && size == 100 * 32, "100 vertices of 32 bytes");
        base = 0x1000, size = 0x50000;
        Check(Narrow(base, size, 100, 199, 32, 40) && base == 0x1000 + 3200 && size == 99 * 32 + 40, "a record reading past its stride");
        base = 0x1000, size = 640;
        Check(!Narrow(base, size, 0, 19, 32, 32) && base == 0x1000 && size == 640, "every vertex: unchanged");
        base = 0x1000, size = 640;
        Check(!Narrow(base, size, 30, 40, 32, 32) && size == 640, "past the range: unchanged");
        base = 0x1000, size = 0x50000;
        Check(Narrow(base, size, 0x2700, 0x3000, 32, 32) && base == 0x1000 + 0x2700 * 32 && base + size == 0x1000 + 0x50000,
            "clipped to the range's end");
    }

    void TestDynamic()
    {
        printf("the dynamic-buffer cache: written since its last draw, then kept\n");
        gpu::vertexnarrow::DynamicRanges cache;
        Check(!cache.Dynamic(0x10000, 0x50000, 5, false), "first seen: static");
        Check(!cache.Dynamic(0x10000, 0x50000, 6, false), "not written since: static");
        Check(cache.Dynamic(0x10000, 0x50000, 7, true), "written since its last draw: dynamic");
        Check(cache.Dynamic(0x10000, 0x50000, 8, false), "once dynamic, kept");
        Check(!cache.Dynamic(0x10000, 0x40000, 9, true), "another size there: starts over");
    }
}

int main()
{
    TestTranslator();
    TestRange();
    TestDynamic();
    if (g_failures)
    {
        printf("%d FAILED\n", g_failures);
        return 1;
    }
    printf("all passed\n");
    return 0;
}

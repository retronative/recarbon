// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// getGradients (texture fetch opcode 18) in the shader translator
// (gpu/shader_translator.cpp). NFS: Carbon's road pixel shader uses it;
// untranslated, every road draw was skipped and what lay behind the road
// showed through (ROADMAP M5). Semantics as Xenia's: (ddx(x), ddy(x),
// ddx(y), ddy(y)) of the first two source components, coarse, pixel
// shaders only. The shader here is assembled by hand from Xenia's ucode.h
// encodings: no game microcode.
// From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/shader_gradients_test.cpp runtime/gpu/shader_translator.cpp runtime/gpu/xenos/ucode.cpp -o build/shader_gradients_test
//   build/shader_gradients_test
#include <gpu/shader_translator.h>

#include <cstdint>
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

    // One exec_end control-flow instruction (address 1, one fetch), then
    // getGradients r1 <- r0.xy, fetch constant 0, 2D.
    std::vector<uint32_t> GradientsShader()
    {
        constexpr uint32_t kExecEnd = 2, kGetTextureGradients = 18;
        // CF: address 1, count 1, sequence 0b01 (a fetch); the second CF of
        // the pair is a nop (all zero).
        const uint32_t cfLo = 1u | (1u << 12) | (1u << 16);
        const uint32_t cfHi = kExecEnd << 12;
        // Fetch dword 0: opcode, src r0, dst r1, const 0, src swizzle xyz.
        const uint32_t f0 = kGetTextureGradients | (0u << 5) | (1u << 12) | ((0u | (1u << 2) | (2u << 4)) << 26);
        // Dword 1: dest swizzle xyzw (3 bits each).
        const uint32_t f1 = 0u | (1u << 3) | (2u << 6) | (3u << 9);
        // Dword 2: dimension 2D (1) at bits 14-15.
        const uint32_t f2 = 1u << 14;
        return { cfLo, cfHi, 0, f0, f1, f2 };
    }
}

int main()
{
    const std::vector<uint32_t> ucode = GradientsShader();

    gpu::TranslatedShader ps = gpu::TranslateShader(gpu::ShaderStage::Pixel, ucode.data(), ucode.size());
    Check(ps.ok, "a pixel shader with getGradients translates");
    if (!ps.ok)
        printf("  (error: %s)\n", ps.error.c_str());
    Check(ps.glsl.find("dFdxCoarse(") != std::string::npos && ps.glsl.find("dFdyCoarse(") != std::string::npos,
        "it takes coarse screen-space derivatives");
    Check(ps.texture2DMask == 0 && ps.texture3DMask == 0 && ps.textureCubeMask == 0, "it reads no texture");

    gpu::TranslatedShader vs = gpu::TranslateShader(gpu::ShaderStage::Vertex, ucode.data(), ucode.size());
    Check(vs.ok, "a vertex shader with getGradients translates");
    Check(vs.glsl.find("dFdxCoarse(") == std::string::npos, "a vertex shader has no derivatives (zero)");

    if (g_failures)
    {
        printf("shader_gradients_test: %d failed\n", g_failures);
        return 1;
    }
    printf("shader_gradients_test: all passed\n");
    return 0;
}

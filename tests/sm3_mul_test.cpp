// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// The shader translator's Shader Model 3 multiply, xmul (gpu/sm3_mul_glsl.h):
// +-0 times anything, NaN and infinity included, is +0; otherwise the IEEE
// product. RECARBON_SELECT_MUL=1 swaps the GLSL that implements it for two
// nested selects (no float compare-sum, no branch). Checked here:
//   - the Xbox 360 rule, bit by bit, on special values (+-0, denormals,
//     +-1, +-FLT_MAX, +-Inf, quiet NaNs with and without payloads), for the
//     current form and the select form, vector and scalar;
//   - the two forms give the same bits on every special pair and on a
//     million random pairs (random bit patterns: NaNs and denormals too);
//   - the GLSL each form is, exactly: the C++ models below mirror it
//     operation for operation, so a change to either text fails here;
//   - the translator: with the flag off its output is exactly as before
//     (the current form, once); with it on only the xmul block differs;
//   - both GLSL outputs written to <dir> for glslangValidator (ctest).
// The GPU may flush denormals to zero; both forms compare against 0.0 in
// the same way, so they flush alike. That part is not testable on the CPU.
// Header only, plus the translator (no Vulkan). From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/sm3_mul_test.cpp runtime/gpu/shader_translator.cpp runtime/gpu/xenos/ucode.cpp -o build/sm3_mul_test
//   build/sm3_mul_test [dir for the GLSL]
#include <gpu/shader_translator.h>
#include <gpu/sm3_mul_glsl.h>

#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
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

    uint32_t Bits(float f)
    {
        uint32_t u;
        memcpy(&u, &f, 4);
        return u;
    }

    float FromBits(uint32_t u)
    {
        float f;
        memcpy(&f, &u, 4);
        return f;
    }

    // The current GLSL (kXmulGlslCurrent), one lane of the vec4 form:
    // zero = (float(abs(a) == 0) + float(abs(b) == 0)) != 0; mix(r, 0, zero),
    // where mix with a bvec selects.
    float CurrentVec(float a, float b)
    {
        float r = a * b;
        float sum = float(std::fabs(a) == 0.0f) + float(std::fabs(b) == 0.0f);
        return sum != 0.0f ? 0.0f : r;
    }

    // Its scalar form: (abs(a) == 0 || abs(b) == 0) ? 0 : r.
    float CurrentScalar(float a, float b)
    {
        float r = a * b;
        return (std::fabs(a) == 0.0f || std::fabs(b) == 0.0f) ? 0.0f : r;
    }

    // The select GLSL (kXmulGlslSelect): r = mix(a * b, 0, a == 0);
    // mix(r, 0, b == 0). Vector and scalar forms are the same per lane.
    float Select(float a, float b)
    {
        float r = a == 0.0f ? 0.0f : a * b;
        return b == 0.0f ? 0.0f : r;
    }

    // The Xbox 360 rule, written from the definition rather than from either form.
    bool RuleHolds(float a, float b, float got)
    {
        if (a == 0.0f || b == 0.0f)  // +0 or -0 (never NaN)
            return Bits(got) == 0x00000000u;
        float ieee = a * b;
        if (std::isnan(ieee))
            return std::isnan(got);
        return Bits(got) == Bits(ieee);
    }

    const std::vector<float>& Specials()
    {
        static const std::vector<float> v = {
            0.0f, -0.0f, FromBits(0x00000001u), FromBits(0x80000001u), FromBits(0x007FFFFFu), FromBits(0x807FFFFFu),
            FLT_MIN, -FLT_MIN, 1.0f, -1.0f, 0.5f, -2.5f, 3.0f, 1e-30f, -1e-30f, 1e30f, -1e30f, FLT_MAX, -FLT_MAX,
            INFINITY, -INFINITY, FromBits(0x7FC00000u), FromBits(0xFFC00000u), FromBits(0x7FC12345u), FromBits(0x7F800001u),
        };
        return v;
    }

    // One shader with no ALU work, assembled by hand (tests/shader_gradients_test.cpp's
    // encodings): exec_end, one fetch (getGradients r1 <- r0.xy). No game microcode.
    std::vector<uint32_t> SmallShader()
    {
        constexpr uint32_t kExecEnd = 2, kGetTextureGradients = 18;
        const uint32_t cfLo = 1u | (1u << 12) | (1u << 16);
        const uint32_t cfHi = kExecEnd << 12;
        const uint32_t f0 = kGetTextureGradients | (0u << 5) | (1u << 12) | ((0u | (1u << 2) | (2u << 4)) << 26);
        const uint32_t f1 = 0u | (1u << 3) | (2u << 6) | (3u << 9);
        const uint32_t f2 = 1u << 14;
        return { cfLo, cfHi, 0, f0, f1, f2 };
    }

    size_t Count(const std::string& s, const char* what)
    {
        size_t n = 0;
        for (size_t at = s.find(what); at != std::string::npos; at = s.find(what, at + 1))
            n++;
        return n;
    }

    bool Write(const std::string& path, const std::string& text)
    {
        FILE* f = fopen(path.c_str(), "w");
        if (!f)
            return false;
        bool ok = fputs(text.c_str(), f) >= 0;
        return fclose(f) == 0 && ok;
    }
}

int main(int argc, char** argv)
{
    printf("---- the Xbox 360 rule on special values (bit level) ----\n");
    {
        int current = 0, scalar = 0, select = 0;
        for (float a : Specials())
            for (float b : Specials())
            {
                current += !RuleHolds(a, b, CurrentVec(a, b));
                scalar += !RuleHolds(a, b, CurrentScalar(a, b));
                select += !RuleHolds(a, b, Select(a, b));
            }
        Check(current == 0, "the current vec4 form follows the rule");
        Check(scalar == 0, "the current scalar form follows the rule");
        Check(select == 0, "the select form follows the rule");
    }
    Check(Bits(Select(0.0f, INFINITY)) == 0, "+0 * +Inf = +0");
    Check(Bits(Select(-0.0f, INFINITY)) == 0, "-0 * +Inf = +0");
    Check(Bits(Select(INFINITY, -0.0f)) == 0, "+Inf * -0 = +0");
    Check(Bits(Select(0.0f, NAN)) == 0, "+0 * NaN = +0");
    Check(Bits(Select(NAN, -0.0f)) == 0, "NaN * -0 = +0");
    Check(Bits(Select(-0.0f, -5.0f)) == 0, "-0 * -5 = +0 (IEEE gives +0 too)");
    Check(Bits(Select(-0.0f, 5.0f)) == 0, "-0 * 5 = +0, not IEEE's -0");
    Check(Bits(Select(3.0f, -0.0f)) == 0, "3 * -0 = +0, not IEEE's -0");
    Check(Bits(Select(-0.0f, -0.0f)) == 0, "-0 * -0 = +0");
    Check(Bits(Select(INFINITY, -2.0f)) == Bits(-INFINITY), "+Inf * -2 = -Inf");
    Check(Bits(Select(-INFINITY, -INFINITY)) == Bits(INFINITY), "-Inf * -Inf = +Inf");
    Check(Bits(Select(FLT_MAX, 2.0f)) == Bits(INFINITY), "FLT_MAX * 2 overflows to +Inf");
    Check(std::isnan(Select(NAN, 1.0f)) && std::isnan(Select(1.0f, NAN)), "NaN * 1 and 1 * NaN: NaN");
    Check(std::isnan(Select(NAN, INFINITY)), "NaN * Inf: NaN");
    Check(Bits(Select(FromBits(0x00000001u), 0.5f)) == 0, "the smallest denormal * 0.5 rounds to +0");
    Check(Bits(Select(FromBits(0x00000001u), 2.0f)) == 2, "a denormal is not zero: doubled, as IEEE (no flush on the CPU)");
    Check(Bits(Select(-1.0f, 1e-45f)) == Bits(-1e-45f), "a negative denormal product keeps its sign");
    volatile float tiny = 1e-30f;  // not folded at compile time
    Check(Bits(Select(-tiny, tiny)) == 0x80000000u, "a product that underflows keeps IEEE's -0 (neither operand zero)");

    printf("---- the current and select forms give the same bits ----\n");
    {
        int differ = 0;
        for (float a : Specials())
            for (float b : Specials())
            {
                uint32_t s = Bits(Select(a, b));
                differ += s != Bits(CurrentVec(a, b)) || s != Bits(CurrentScalar(a, b));
            }
        Check(differ == 0, "every special pair (625)");
        std::mt19937 rng(20261028);
        int random = 0;
        for (int i = 0; i < 1000000; i++)
        {
            float a = FromBits(rng()), b = FromBits(rng());
            if (i & 1)  // half the pairs: one operand a signed zero
                (i & 2 ? a : b) = i & 4 ? -0.0f : 0.0f;
            uint32_t s = Bits(Select(a, b));
            random += s != Bits(CurrentVec(a, b)) || s != Bits(CurrentScalar(a, b));
        }
        Check(random == 0, "a million random bit patterns, half with a signed zero");
    }

    printf("---- the GLSL texts the models mirror ----\n");
    const std::string current = gpu::sm3::kXmulGlslCurrent, select = gpu::sm3::kXmulGlslSelect;
    Check(current ==
              "vec4 xmul(vec4 a, vec4 b)\n{\n    vec4 r = a * b;\n"
              "    bvec4 zero = notEqual(vec4(equal(abs(a), vec4(0.0))) + vec4(equal(abs(b), vec4(0.0))), vec4(0.0));\n"
              "    return mix(r, vec4(0.0), zero);\n}\n"
              "float xmul(float a, float b)\n{\n    float r = a * b;\n"
              "    return (abs(a) == 0.0 || abs(b) == 0.0) ? 0.0 : r;\n}\n",
        "the current form is CurrentVec and CurrentScalar");
    Check(select ==
              "vec4 xmul(vec4 a, vec4 b)\n{\n    vec4 r = mix(a * b, vec4(0.0), equal(a, vec4(0.0)));\n"
              "    return mix(r, vec4(0.0), equal(b, vec4(0.0)));\n}\n"
              "float xmul(float a, float b)\n{\n    float r = mix(a * b, 0.0, a == 0.0);\n"
              "    return mix(r, 0.0, b == 0.0);\n}\n",
        "the select form is Select (mix with a bool selects)");
    Check(Count(gpu::ShaderCommonGlsl(), current.c_str()) == 1, "the common GLSL holds the current form once");
    Check(Count(gpu::ShaderCommonGlsl(), "xmul(") == 2, "and no other xmul definition");

    printf("---- the translator ----\n");
    const std::vector<uint32_t> ucode = SmallShader();
    for (gpu::ShaderStage stage : { gpu::ShaderStage::Pixel, gpu::ShaderStage::Vertex })
    {
        gpu::TranslatedShader off = gpu::TranslateShader(stage, ucode.data(), ucode.size(), false);
        gpu::TranslatedShader on = gpu::TranslateShader(stage, ucode.data(), ucode.size(), true);
        if (stage == gpu::ShaderStage::Vertex)  // getGradients is pixel-only: the vertex shader may fail; skip it then
        {
            if (!off.ok)
                continue;
        }
        Check(off.ok && on.ok, "both translate");
        Check(Count(off.glsl, current.c_str()) == 1 && Count(off.glsl, select.c_str()) == 0, "off: the current form, once");
        Check(Count(on.glsl, select.c_str()) == 1 && Count(on.glsl, current.c_str()) == 0, "on: the select form, once");
        std::string back = on.glsl;
        if (const size_t at = back.find(select); at != std::string::npos)
            back.replace(at, select.size(), current);
        Check(back == off.glsl, "on differs from off only in the xmul block");
        Check(off.glsl.find(gpu::ShaderCommonGlsl()) == 0, "off: the output starts with the common GLSL as before");
        if (stage == gpu::ShaderStage::Pixel && argc > 1)
        {
            Check(Write(std::string(argv[1]) + "/sm3_mul_off.frag", off.glsl), "wrote sm3_mul_off.frag");
            Check(Write(std::string(argv[1]) + "/sm3_mul_on.frag", on.glsl), "wrote sm3_mul_on.frag");
        }
    }
    Check(!gpu::SelectMulEnabled() || std::getenv("RECARBON_SELECT_MUL"), "the flag is off unless RECARBON_SELECT_MUL is set");

    if (g_failures)
    {
        printf("%d FAILED\n", g_failures);
        return 1;
    }
    printf("all passed\n");
    return 0;
}

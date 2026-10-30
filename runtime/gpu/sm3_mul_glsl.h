// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// The shader translator's Shader Model 3 multiply (gpu/shader_translator.cpp):
// +-0 (or a denormal, where the GPU flushes it) times anything, NaN and
// infinity included, is +0; otherwise the IEEE product. Translated shaders
// call xmul for every multiply, mad and dot product, so its form sets much of
// their ALU cost (measured on a MacBook Air M5, October 2026).
//
// kXmulGlslCurrent is the form shaders have always had (in ShaderCommonGlsl):
// a compare-and-sum per operand, then a select (vec4), or a short-circuit ||
// that compiles to a branch (scalar). kXmulGlslSelect gives the same bits for
// every input with two nested selects and no branch. RECARBON_SELECT_MUL=1
// (an experiment, off by default) translates with it. tests/sm3_mul_test.cpp
// pins both texts and checks their results bit by bit.
#pragma once

namespace gpu::sm3
{
    inline constexpr const char* kXmulGlslCurrent = R"(vec4 xmul(vec4 a, vec4 b)
{
    vec4 r = a * b;
    bvec4 zero = notEqual(vec4(equal(abs(a), vec4(0.0))) + vec4(equal(abs(b), vec4(0.0))), vec4(0.0));
    return mix(r, vec4(0.0), zero);
}
float xmul(float a, float b)
{
    float r = a * b;
    return (abs(a) == 0.0 || abs(b) == 0.0) ? 0.0 : r;
}
)";

    inline constexpr const char* kXmulGlslSelect = R"(vec4 xmul(vec4 a, vec4 b)
{
    vec4 r = mix(a * b, vec4(0.0), equal(a, vec4(0.0)));
    return mix(r, vec4(0.0), equal(b, vec4(0.0)));
}
float xmul(float a, float b)
{
    float r = mix(a * b, 0.0, a == 0.0);
    return mix(r, 0.0, b == 0.0);
}
)";
}

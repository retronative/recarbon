// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// Tests for kernel::ParseThreadName (runtime/kernel/raise_exception.h): which
// RtlRaiseException records are thread-name requests. Header only; from the
// repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/raise_exception_test.cpp -o build/raise_exception_test
//   build/raise_exception_test
#include <kernel/raise_exception.h>

#include <cstdio>
#include <initializer_list>

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { g_failures++; printf("  FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } while (0)

static kernel::ExceptionRecordView Record(uint32_t code, uint32_t numberParameters, const uint32_t* information)
{
    return { .code = code, .flags = 0, .record = 0, .address = 0x82000000,
        .numberParameters = numberParameters, .information = information };
}

int main()
{
    using kernel::ParseThreadName;
    using kernel::ThreadNameRequest;

    // A THREADNAME_INFO: dwType, szName, dwThreadID, dwFlags.
    {
        const uint32_t info[4] = { 0x1000, 0x82001234, 0x0000000F, 0 };
        ThreadNameRequest out = { 0xDEADBEEF, 0xDEADBEEF };
        CHECK(ParseThreadName(Record(0x406D1388, 4, info), out), "a thread-name request");
        CHECK(out.namePointer == 0x82001234, "name pointer 0x%08X", out.namePointer);
        CHECK(out.threadId == 0x0000000F, "thread id 0x%08X", out.threadId);
    }

    // 0xFFFFFFFF, the calling thread, is kept as it is.
    {
        const uint32_t info[4] = { 0x1000, 0x40001000, 0xFFFFFFFF, 0 };
        ThreadNameRequest out = {};
        CHECK(ParseThreadName(Record(0x406D1388, 4, info), out), "the calling thread");
        CHECK(out.namePointer == 0x40001000, "name pointer 0x%08X", out.namePointer);
        CHECK(out.threadId == 0xFFFFFFFF, "thread id 0x%08X", out.threadId);
    }

    // Three parameters are enough (dwFlags isn't read).
    {
        const uint32_t info[3] = { 0x1000, 0x82001234, 7 };
        ThreadNameRequest out = {};
        CHECK(ParseThreadName(Record(0x406D1388, 3, info), out) && out.threadId == 7, "three parameters");
    }

    // The thread-name code with parameters that aren't a THREADNAME_INFO.
    for (uint32_t type : { 0u, 0x0FFFu, 0x1001u, 0xFFFFFFFFu })
    {
        const uint32_t info[4] = { type, 0x82001234, 0xFFFFFFFF, 0 };
        ThreadNameRequest out = { 1, 2 };
        CHECK(!ParseThreadName(Record(0x406D1388, 4, info), out), "dwType 0x%X isn't one", type);
        CHECK(out.namePointer == 1 && out.threadId == 2, "dwType 0x%X: out untouched", type);
    }

    // Too few parameters; information isn't read past numberParameters.
    for (uint32_t count : { 0u, 1u, 2u })
    {
        const uint32_t info[4] = { 0x1000, 0x82001234, 0xFFFFFFFF, 0 };
        ThreadNameRequest out = { 1, 2 };
        CHECK(!ParseThreadName(Record(0x406D1388, count, count ? info : nullptr), out), "%u parameters", count);
        CHECK(out.namePointer == 1 && out.threadId == 2, "%u parameters: out untouched", count);
    }

    // Other codes, even with a THREADNAME_INFO's parameters: MSVC's C++
    // exception, an access violation, a breakpoint.
    for (uint32_t code : { 0xE06D7363u, 0xC0000005u, 0x80000003u, 0x406D1389u })
    {
        const uint32_t info[4] = { 0x1000, 0x82001234, 0xFFFFFFFF, 0 };
        ThreadNameRequest out = { 1, 2 };
        CHECK(!ParseThreadName(Record(code, 4, info), out), "code 0x%08X isn't one", code);
        CHECK(out.namePointer == 1 && out.threadId == 2, "code 0x%08X: out untouched", code);
    }

    printf("%s (%d failures)\n", g_failures ? "FAILED" : "passed", g_failures);
    return g_failures ? 1 : 0;
}

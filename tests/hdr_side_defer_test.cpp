// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// gpu/hdr_side_defer.h: HDR Output's float copy of a resolve (gpu/renderer.cpp's HdrSide) waits
// until something could change its target: the next pass, the target's destruction, the next side
// copy, a reset of HDR Output's images. The present reads the target itself when the copy it would
// read is still waiting (the copy is then never made). Header only: no Vulkan.
// From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/hdr_side_defer_test.cpp -o build/hdr_side_defer_test && build/hdr_side_defer_test
#include <gpu/hdr_side_defer.h>

#include <cstdio>

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

    struct Target {};
    struct Side {};
    struct Copy
    {
        const Target* rt;
        Side* side;
        int id;
    };
    using Pending = gpu::hdrsidedefer::Pending<Copy>;

    void TestWaitsUntilAPass()
    {
        printf("a copy waits, and is recorded before the next pass\n");
        Target t;
        Side s;
        Pending p;
        Check(!p.Waiting(), "nothing waits at first");
        Check(!p.Defer({ &t, &s, 1 }), "nothing to record before the first");
        Check(p.Waiting(), "the copy waits");
        auto c = p.Take();
        Check(c && c->id == 1, "a pass takes it, to record first");
        Check(!p.Waiting() && !p.Take(), "then nothing waits");
    }

    void TestOneAtATime()
    {
        printf("a second copy records the first, in order\n");
        Target t;
        Side a, b;
        Pending p;
        p.Defer({ &t, &a, 1 });
        auto first = p.Defer({ &t, &b, 2 });
        Check(first && first->id == 1, "the first is recorded before the second waits");
        Check(p.Waiting() && p.Take()->id == 2, "the second waits");
    }

    void TestTargetGone()
    {
        printf("a target destroyed: its waiting copy is recorded first, another's stays\n");
        Target t, other;
        Side s;
        Pending p;
        p.Defer({ &t, &s, 1 });
        Check(!p.TakeFor(&other), "another target: nothing");
        Check(p.Waiting(), "still waiting");
        auto c = p.TakeFor(&t);
        Check(c && c->id == 1, "its own: recorded");
        Check(!p.Waiting(), "then nothing waits");
    }

    void TestPresentReadsTarget()
    {
        printf("the present reads the target for the side whose copy waits\n");
        Target t;
        Side front, other;
        Pending p;
        p.Defer({ &t, &front, 1 });
        auto read = p.ReadAtPresent(&front);
        Check(read && read->rt == &t, "the front's copy waits: the present reads its target");
        Check(!p.Waiting(), "and the copy is never made");

        p.Defer({ &t, &other, 2 });
        Check(!p.ReadAtPresent(&front), "another side's copy: the present reads the front's side");
        Check(p.Waiting(), "that copy still waits (the caller records it before reading the side)");
        Check(!p.ReadAtPresent(nullptr), "no side: nothing");
    }

    void TestReset()
    {
        printf("HDR Output's images reset: a waiting copy is dropped\n");
        Target t;
        Side s;
        Pending p;
        p.Defer({ &t, &s, 1 });
        p.Drop();
        Check(!p.Waiting() && !p.Take(), "nothing waits after a reset");
    }
}

int main()
{
    TestWaitsUntilAPass();
    TestOneAtATime();
    TestTargetGone();
    TestPresentReadsTarget();
    TestReset();
    if (g_failures)
    {
        printf("%d FAILED\n", g_failures);
        return 1;
    }
    printf("all passed\n");
    return 0;
}

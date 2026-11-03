// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// game/render_queue_log.h: RECARBON_RQ_LOG's switch and its [rq] lines (the soak's exposure
// measure: recording windows, publishes, entries played).
// From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/render_queue_log_test.cpp -o build/render_queue_log_test && build/render_queue_log_test
#include <game/render_queue_log.h>

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

    void Equal(const std::string& got, const std::string& want, const char* what)
    {
        if (got != want)
            printf("  got:  %s\n  want: %s\n", got.c_str(), want.c_str());
        Check(got == want, what);
    }

    using namespace game::renderqueue;
}

int main()
{
    // Off unless asked for: unset, empty and "0" are off; anything else is on.
    Check(!LogEnabled(nullptr), "unset is off");
    Check(!LogEnabled(""), "empty is off");
    Check(!LogEnabled("0"), "0 is off");
    Check(LogEnabled("1"), "1 is on");
    Check(LogEnabled("yes"), "another value is on");
    Check(LogEnabled("00"), "only exactly 0 is off");

    // The interval is the difference of two snapshots of the running totals, field by field.
    const Counts before{ 3, 400, 390 }, after{ 7, 1000, 1000 };
    const Counts interval = after - before;
    Check(interval.windows == 4 && interval.publishes == 600 && interval.played == 610, "interval = after - before, per field");

    Equal(IntervalLine(10.04, interval, after),
        "[rq] 10 s: 4 recording windows, 600 publishes, 610 entries played (-10 not played); "
        "since launch 7 recording windows, 1000 publishes, 1000 entries played (0 not played)",
        "the 10 s line: the interval, then the totals; played ahead of publishes shows negative");
    Equal(IntervalLine(29.6, Counts{}, after).substr(0, 10), "[rq] 30 s:", "a longer interval (no window for a while) is rounded and named");
    Equal(TotalLine(1000.2, Counts{ 40, 120000, 119990 }),
        "[rq] total over 1000 s: 40 recording windows, 120000 publishes, 119990 entries played (10 not played)",
        "the exit line");

    printf("%s\n", g_failures ? "render_queue_log_test: FAILED" : "render_queue_log_test: all passed");
    return g_failures ? 1 : 0;
}

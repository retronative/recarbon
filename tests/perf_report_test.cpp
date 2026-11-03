// ReCarbon: the performance report's format, quantiles, switch policy, scrub
// and denylist (runtime/report/perf_report.h, perf_scrub.h), and the present
// ring (runtime/video/present_ring.h), all header only. From the repo root:
//   clang++ -std=c++20 -O2 -UNDEBUG -Iruntime tests/perf_report_test.cpp -o build/perf_report_test
//   build/perf_report_test [file]   (writes the poisoned sample's report to <file>)
// Expected values are worked by hand from the formula, not taken from the
// output. The ring's Push is timed and printed (not checked: a timing check
// would make this a serial test).
#include <report/perf_report.h>
#include <video/present_ring.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <variant>
#include <vector>

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { g_failures++; printf("  FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } while (0)

using namespace report;

// A small JSON reader, enough to check the document's shape.
struct Value;
using Object = std::vector<std::pair<std::string, std::shared_ptr<Value>>>;
using Array = std::vector<std::shared_ptr<Value>>;
struct Value
{
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> v;
    const Value* Get(const std::string& key) const
    {
        if (auto* o = std::get_if<Object>(&v))
            for (const auto& [k, val] : *o)
                if (k == key)
                    return val.get();
        return nullptr;
    }
};

struct Reader
{
    const std::string& s;
    size_t i = 0;
    bool ok = true;
    void Ws() { while (i < s.size() && std::isspace(uint8_t(s[i]))) i++; }
    bool Eat(char c) { Ws(); if (i < s.size() && s[i] == c) { i++; return true; } return false; }
    std::string Str()
    {
        std::string out;
        if (!Eat('"')) { ok = false; return out; }
        while (i < s.size() && s[i] != '"')
        {
            if (uint8_t(s[i]) < 0x20) { ok = false; return out; }
            if (s[i] == '\\')
            {
                if (++i >= s.size()) { ok = false; return out; }
                char e = s[i++];
                if (e == 'u') { if (i + 4 > s.size()) { ok = false; return out; } out += '?'; i += 4; }
                else if (e == 'n') out += '\n';
                else if (e == 't') out += '\t';
                else if (e == '"' || e == '\\' || e == '/') out += e;
                else { ok = false; return out; }
            }
            else
                out += s[i++];
        }
        if (i >= s.size()) ok = false;
        i++;
        return out;
    }
    std::shared_ptr<Value> Parse()
    {
        auto val = std::make_shared<Value>();
        Ws();
        if (i >= s.size()) { ok = false; return val; }
        char c = s[i];
        if (c == '{')
        {
            i++;
            Object o;
            if (!Eat('}'))
            {
                do { std::string k = Str(); if (!Eat(':')) ok = false; o.emplace_back(k, Parse()); } while (ok && Eat(','));
                if (ok && !Eat('}')) ok = false;
            }
            val->v = std::move(o);
        }
        else if (c == '[')
        {
            i++;
            Array a;
            if (!Eat(']'))
            {
                do a.push_back(Parse()); while (ok && Eat(','));
                if (ok && !Eat(']')) ok = false;
            }
            val->v = std::move(a);
        }
        else if (c == '"')
            val->v = Str();
        else if (s.compare(i, 4, "null") == 0) { i += 4; val->v = nullptr; }
        else if (s.compare(i, 4, "true") == 0) { i += 4; val->v = true; }
        else if (s.compare(i, 5, "false") == 0) { i += 5; val->v = false; }
        else
        {
            size_t start = i;
            while (i < s.size() && (std::isdigit(uint8_t(s[i])) || std::strchr("+-.eE", s[i]))) i++;
            if (i == start) { ok = false; return val; }
            val->v = std::stod(s.substr(start, i - start));
        }
        return val;
    }
};

// The fenced JSON of a report ("" when there is none).
static std::string Fence(const std::string& text)
{
    size_t open = text.find("```json\n");
    if (open == std::string::npos)
        return {};
    open += 8;
    size_t close = text.find("\n```", open);
    return close == std::string::npos ? std::string() : text.substr(open, close - open);
}

// Every object with a "status" is a metric: one of three, a "source", and a
// null value exactly when unavailable (with a "why"). Every leaf under the
// sections is inside a metric, but for the few plain fields listed.
static void CheckMetrics(const Value& v, const std::string& path, bool inMetric, int& metrics)
{
    if (auto* o = std::get_if<Object>(&v.v))
    {
        bool metric = v.Get("status") != nullptr;
        if (metric)
        {
            metrics++;
            auto* status = std::get_if<std::string>(&v.Get("status")->v);
            CHECK(status && (*status == "measured" || *status == "estimated" || *status == "unavailable"), "%s: status", path.c_str());
            const Value* value = v.Get("value");
            CHECK(value, "%s: no value", path.c_str());
            CHECK(v.Get("source") && std::holds_alternative<std::string>(v.Get("source")->v), "%s: no source", path.c_str());
            if (status && value)
            {
                const bool null = std::holds_alternative<std::nullptr_t>(value->v);
                CHECK(null == (*status == "unavailable"), "%s: value is null iff unavailable", path.c_str());
                if (*status == "unavailable")
                    CHECK(v.Get("why") != nullptr, "%s: unavailable without why", path.c_str());
            }
        }
        for (const auto& [k, child] : *o)
            CheckMetrics(*child, path + "." + k, inMetric || metric, metrics);
    }
    else if (auto* a = std::get_if<Array>(&v.v))
    {
        for (const auto& child : *a)
            CheckMetrics(*child, path + "[]", inMetric, metrics);
    }
    else if (!inMetric)
    {
        // Leaves outside a metric: the header fields, the build, each
        // section's "window", and the switches (strings).
        static const char* plain[] = { ".schema", ".generated", ".trigger", ".session_s", ".build.", ".window", ".flags." };
        bool ok = false;
        for (const char* p : plain)
            ok |= path.find(p) != std::string::npos;
        CHECK(ok, "%s: a bare value outside a metric", path.c_str());
    }
}

static const char* kTopKeys[] = { "schema", "generated", "trigger", "session_s", "build", "system", "display", "resolution", "frames",
    "display_events", "cp", "gpu", "hitches", "thermal", "cpu", "warnings", "flags" };

// The whole document: it parses, has the schema and only the known top-level
// keys, and every metric is well formed. Returns the metric count.
static int CheckDocument(const std::string& text, std::shared_ptr<Value>* out = nullptr)
{
    std::string json = Fence(text);
    CHECK(!json.empty(), "no fenced JSON");
    CHECK(text.find("--- machine-readable (recarbon-perf-report/1) ---\n```json\n") != std::string::npos, "no separator line");
    Reader r{ json };
    auto doc = r.Parse();
    r.Ws();
    CHECK(r.ok && r.i == json.size(), "JSON doesn't parse (at %zu of %zu)", r.i, json.size());
    if (!r.ok)
        return 0;
    const Value* schema = doc->Get("schema");
    CHECK(schema && std::get<std::string>(schema->v) == "recarbon-perf-report/1", "schema");
    if (auto* o = std::get_if<Object>(&doc->v))
    {
        for (const auto& [k, v] : *o)
            CHECK(std::find(std::begin(kTopKeys), std::end(kTopKeys), k) != std::end(kTopKeys), "unknown top-level key %s", k.c_str());
        CHECK(o->size() == std::size(kTopKeys), "%zu top-level keys", o->size());
    }
    int metrics = 0;
    CheckMetrics(*doc, "", false, metrics);
    if (out)
        *out = doc;
    return metrics;
}

static double Num(const Value* v)
{
    auto* d = v ? std::get_if<double>(&v->v) : nullptr;
    return d ? *d : NAN;
}

// A Mac at 60 fps for 20 s, with a hitch: every value is plain.
static perf::Snapshot Plain()
{
    perf::Snapshot s;
    s.generated = "2026-10-31T01:40:12Z";
    s.trigger = "settings-row";
    s.version = "0.1.4";
    s.commit = "abc1234";
    s.build = "v0.1.4 (abc1234), built 2026-10-31 00:00 UTC, clang 21.1.0, macOS arm64, Release";
    s.sessionS = 42.5;
    s.machine = "Mac16,13";
    s.chip = "Apple M5";
    s.perfCores = 4, s.effCores = 6;
    s.memoryGb = 24;
    s.os = "macOS 27.0 (25A354)";
    s.gpu = "Apple M5 (integrated), MoltenVK 1.3.0, Vulkan 1.3.296";
    s.displayKnown = true;
    s.modeW = 2560, s.modeH = 1664, s.hz = 60, s.windowW = 2560, s.windowH = 1664, s.swapW = 2560, s.swapH = 1664;
    s.fullscreen = true, s.vsync = true, s.presentsTimed = true;
    s.resolutionSetting = "Auto", s.aaSetting = "On", s.frameRateSetting = "60", s.vsyncSetting = "On";
    s.frameRateNow = 60;
    s.appliedKnown = true, s.appliedScale = "2x", s.appliedW = 2560, s.appliedH = 1440;
    int64_t t = 1'000'000'000;
    for (int i = 0; i < 1200; i++)
    {
        s.presentedNs.push_back(t);
        t += i == 600 ? 64'000'000 : 16'666'667;
    }
    s.framesPresented = 2000;
    s.dropped = 3, s.repeated = 5, s.shown = 1990;
    s.tickKnown = true;
    s.guestFps = 59.9, s.worstMs = 64, s.cpBusyMs = 4.1, s.draws = 1180, s.gpuBusyMs = 9.8, s.gpuExecMs = 8.9, s.gpuTimestamps = true;
    s.swaps = 2000, s.lateFrames = 2, s.cpBusyTotalMs = 8200;
    s.waitTotalMs[0] = 3302, s.waitTotalMs[1] = 1210.5;
    s.thermalNow = "fair";
    s.thermalHistory = { { 0.0, "nominal" }, { 30.5, "fair" } };
    s.pressureKnown = true, s.pressureLevel = 1;
    s.cpuKnown = true, s.userMs = 39120, s.systemMs = 4410;
    s.threadsKnown = true, s.threads = { { "recarbon-cp", 14020 }, { "recarbon-present", 210 } };
    s.logKnown = true, s.logLines = 400, s.warnings = { { "renderer", 2 } };
    s.env = { { "RECARBON_GPU_WAIT_BACKOFF", "0" } };
    s.settings = { { "video.frame_rate", "60" } };
    return s;
}

// The same, with private things in every string a module could hand it.
static perf::Snapshot Poisoned()
{
    perf::Snapshot s = Plain();
    s.machine = "Mac16,13 /Users/alice/Desktop screen";
    s.gpu = "Apple M5 at sub_82001234";
    s.os = "macOS 27.0 at 0x82ABCDEF";
    s.chip = "Apple M5 of Alice Example";
    s.frameRateNote = "Now 30 fps: see ~/Library/Logs and /private/var/folders/x/y";
    s.threads.push_back({ "alice-worker", 5 });
    s.threads.push_back({ "Alices-MacBook-Pro", 1 });
    s.env.push_back({ "RECARBON_FRAME_LOG", perf::EnvValue("RECARBON_FRAME_LOG", "/Users/alice/f.csv") });
    s.env.push_back({ "RECARBON_HOME_NOTE", "/Users/alice/notes" });  // as if a policy let a path through
    s.warnings.push_back({ "zpd 0x8201ABCD", 1 });
    return s;
}

static perfscrub::Inputs AliceInputs()
{
    return { { "/Users/alice", "/System/Volumes/Data/Users/alice" }, { "Alice Example" }, { "alice", "Alices-MacBook-Pro" } };
}

static void TestQuantiles()
{
    printf("quantiles\n");
    std::vector<double> five = { 10, 20, 30, 40, 50 };
    // int(0.5*4+0.5) = 2 -> 30; int(0.99*4+0.5) = int(4.46) = 4 -> 50; p0 -> 10; p100 -> 50.
    CHECK(perf::Quantile(five, 50) == 30, "p50 %g", perf::Quantile(five, 50));
    CHECK(perf::Quantile(five, 99) == 50, "p99 %g", perf::Quantile(five, 99));
    CHECK(perf::Quantile(five, 0) == 10, "p0");
    CHECK(perf::Quantile(five, 100) == 50, "p100");
    // int(0.95*4+0.5) = int(4.3) = 4 -> 50; int(0.25*4+0.5) = int(1.5) = 1 -> 20.
    CHECK(perf::Quantile(five, 95) == 50, "p95 of five");
    CHECK(perf::Quantile(five, 25) == 20, "p25 of five");
    std::vector<double> thousand;
    for (int i = 1; i <= 1000; i++)
        thousand.push_back(i);
    // int(0.95*999+0.5) = int(949.55) = 949 -> 950; int(0.99*999+0.5) = int(989.51) = 989 -> 990;
    // int(0.5*999+0.5) = int(500) = 500 -> 501.
    CHECK(perf::Quantile(thousand, 95) == 950, "p95 %g", perf::Quantile(thousand, 95));
    CHECK(perf::Quantile(thousand, 99) == 990, "p99 %g", perf::Quantile(thousand, 99));
    CHECK(perf::Quantile(thousand, 50) == 501, "p50 %g", perf::Quantile(thousand, 50));
    CHECK(perf::Quantile(std::vector<double>{ 7 }, 99) == 7, "one value");
    CHECK(std::isnan(perf::Quantile({}, 50)), "none");

    // Status by intervals: 59 nothing, 60 an estimate, 599 still, 600 measured.
    CHECK(perf::FrameStatus(59) == perf::Status::Unavailable, "59");
    CHECK(perf::FrameStatus(60) == perf::Status::Estimated, "60");
    CHECK(perf::FrameStatus(599) == perf::Status::Estimated, "599");
    CHECK(perf::FrameStatus(600) == perf::Status::Measured, "600");

    // Intervals: 10, 20, 30, 40, 50 ms; thresholds are strict (> 25, > 33.4, > 50).
    std::vector<int64_t> at = { 0, 10'000'000, 30'000'000, 60'000'000, 100'000'000, 150'000'000 };
    perf::IntervalStats iv = perf::Intervals(at);
    CHECK(iv.n == 5 && iv.p50 == 30 && iv.p99 == 50 && iv.maxMs == 50, "n %zu p50 %g p99 %g max %g", iv.n, iv.p50, iv.p99, iv.maxMs);
    CHECK(iv.over25 == 3 && iv.over33 == 2 && iv.over50 == 0, "over %llu %llu %llu", (unsigned long long)iv.over25,
        (unsigned long long)iv.over33, (unsigned long long)iv.over50);
    // 5 intervals in 150 ms: 33.33 fps.
    CHECK(std::fabs(iv.meanFps - 5000.0 / 150.0) < 1e-9 && std::fabs(iv.durationS - 0.15) < 1e-12, "fps %g", iv.meanFps);
    CHECK(iv.status == perf::Status::Unavailable, "5 intervals are too few");
    // A 749 ms frame is not clamped.
    CHECK(perf::Intervals({ 0, 749'000'000 }).maxMs == 749, "no clamp");
}

static void TestEnvPolicy()
{
    printf("environment policy\n");
    CHECK(perf::EnvValue("RECARBON_GPU_WAIT_BACKOFF", "0") == "0", "an experimental switch is valued");
    CHECK(perf::EnvValue("RECARBON_THERMAL_SIM", "60:serious,120:fair") == "60:serious,120:fair", "a list");
    CHECK(perf::EnvValue("RECARBON_SOMETHING_NEW", "1") == "(set)", "not on the list");
    CHECK(perf::EnvValue("RECARBON_FRAME_LOG", "/Users/alice/f.csv") == "(set)", "a path variable");
    const std::string_view settingsVars[] = { "RECARBON_FRAME_LOG", "RECARBON_VSYNC" };
    CHECK(perf::EnvValue("RECARBON_FRAME_LOG", "1", settingsVars) == "(set)", "never valued, even when listed");
    CHECK(perf::EnvValue("RECARBON_VSYNC", "0", settingsVars) == "0", "a setting's variable");
    CHECK(perf::EnvValue("RECARBON_VSYNC", "0") == "(set)", "a setting's variable not passed");
    CHECK(perf::EnvValue("RECARBON_RENDER_SCALE", "../x/y") == "(set)", "a value with a slash");
    CHECK(perf::EnvValue("RECARBON_RENDER_SCALE", "2 3") == "(set)", "a value with a space");
    CHECK(perf::EnvValue("RECARBON_RENDER_SCALE", std::string(41, '1')) == "(set)", "a long value");
}

static void TestThreadLabels()
{
    printf("thread labels\n");
    CHECK(perf::ThreadLabel("recarbon-cp") == "recarbon-cp", "a runtime thread");
    CHECK(perf::ThreadLabel("g3-82123456") == "guest-3", "a guest thread: %s", perf::ThreadLabel("g3-82123456").c_str());
    CHECK(perf::ThreadLabel("g12-8212ABCD") == "guest-12", "two digits");
    CHECK(perf::ThreadLabel("Render Thread") == "Render Thread", "a name the game gave");
    CHECK(perf::ThreadLabel("worker 82123456") == "other", "a hex run");
    CHECK(perf::ThreadLabel("g3-8212345") == "other", "seven digits: not a guest thread, and a hex run");
    CHECK(perf::ThreadLabel("name/with/slash") == "other", "a slash");
    CHECK(perf::ThreadLabel(std::string(32, 'x')) == "other", "too long");
    CHECK(perf::ThreadLabel("") == "(unnamed)", "none");
}

static void TestWarnings()
{
    printf("warnings\n");
    size_t lines = 0;
    auto w = perf::CountWarnings("[renderer] pipeline failed\n[vblank] fine\n[renderer] warning: x\n[audio] gap error\nno tag failed\n[Odd Tag] failed\n\n", &lines);
    CHECK(lines == 6, "lines %zu", lines);
    std::map<std::string, uint64_t> m(w.begin(), w.end());
    CHECK(m.size() == 3 && m["renderer"] == 2 && m["audio"] == 1 && m["(untagged)"] == 2, "counts: %zu", m.size());
    // Most first; a tie by name ("(untagged)" before "renderer").
    CHECK(w.size() == 3 && w[0].first == "(untagged)" && w[1].first == "renderer" && w[2].first == "audio", "order");
}

static void TestScrub()
{
    printf("scrub\n");
    const perfscrub::Inputs in = AliceInputs();
    CHECK(perfscrub::Scrub("at /Users/alice/Desktop screen", in) == "at <path> screen", "%s", perfscrub::Scrub("at /Users/alice/Desktop screen", in).c_str());
    CHECK(perfscrub::Scrub("/Users/alice", in) == "~", "home alone");
    CHECK(perfscrub::Scrub("/Users/alice2/x", in) == "<path>", "another user's folder");
    CHECK(perfscrub::Scrub("x /mnt/Users/alice y", in).find("alice") == std::string::npos, "a home inside a longer path: the name goes");
    CHECK(perfscrub::Scrub("in ~/Library", in) == "in <path>", "a ~ path");
    CHECK(perfscrub::Scrub("~ alone", in) == "~ alone", "a lone ~ stays");
    CHECK(perfscrub::Scrub("sub_82001234 and 0x82ABCDEF", in) == "<guest-address> and <guest-address>", "guest addresses");
    CHECK(perfscrub::Scrub("0x82ABCDEF0 sub_8200123 xsub_82001234", in) == "0x82ABCDEF0 sub_8200123 xsub_82001234", "not addresses");
    CHECK(perfscrub::Scrub("ALICE and Example, Alices-MacBook-Pro", in) == "<user> and <user>, <user>", "%s",
        perfscrub::Scrub("ALICE and Example, Alices-MacBook-Pro", in).c_str());
    CHECK(perfscrub::Scrub("Apple M1 Pro", in) == "Apple M1 Pro", "a host name's words stay");
    CHECK(perfscrub::Scrub("malice", in) == "malice", "a name inside a word stays");
    perfscrub::Inputs shortName{ {}, { "Al M5" }, { "m5" } };
    CHECK(perfscrub::Scrub("Apple M5", shortName) == "Apple M5", "names shorter than 3 are left");

    CHECK(perfscrub::FindForbidden("Apple M5 at 16.7 / 17.4 ms, ms/frame, sorted[min(n-1, int(p/100*(n-1)+0.5))]", in).empty(), "clean text");
    CHECK(!perfscrub::FindForbidden("x /Applications/ReCarbon.app y", in).empty(), "a path");
    CHECK(!perfscrub::FindForbidden("\"/opt/x\"", in).empty(), "a quoted path");
    CHECK(!perfscrub::FindForbidden("~/x", in).empty(), "a ~ path");
    CHECK(!perfscrub::FindForbidden("sub_82001234", in).empty(), "an address");
    CHECK(!perfscrub::FindForbidden("by alice", in).empty(), "a name");
    for (const char* bad : { "slot0.sav", "settings.toml", "profiles/keyboard", "default.xex", "carbon.iso" })
        CHECK(!perfscrub::FindForbidden(bad, in).empty(), "%s", bad);
}

static void TestReports()
{
    printf("reports\n");
    const perfscrub::Inputs in = AliceInputs();
    std::string refused;

    // A plain report: well formed, numbers where they should be.
    std::optional<std::string> plain = perf::Build(Plain(), in, refused);
    CHECK(plain && refused.empty(), "plain refused: %s", refused.c_str());
    if (plain)
    {
        std::shared_ptr<Value> doc;
        int metrics = CheckDocument(*plain, &doc);
        CHECK(metrics >= 30, "%d metrics", metrics);
        if (doc)
        {
            // 1199 intervals: 1198 at 16.666667 ms, one at 64 ms. p50/p95/p99 16.67, max 64.
            const Value* interval = doc->Get("frames") ? doc->Get("frames")->Get("interval_ms") : nullptr;
            const Value* value = interval ? interval->Get("value") : nullptr;
            CHECK(value && Num(value->Get("p99")) == 16.67 && Num(value->Get("max")) == 64.0, "interval values");
            CHECK(interval && std::get<std::string>(interval->Get("status")->v) == "measured", "1199 intervals are measured");
            CHECK(Num(doc->Get("frames")->Get("over_50ms")->Get("value")) == 1, "one over 50 ms");
            CHECK(Num(doc->Get("cp")->Get("wait_ms_total")->Get("value")->Get("regmem")) == 3302, "regmem total");
        }
        CHECK(plain->starts_with("ReCarbon performance report (v0.1.4 abc1234, 2026-10-31T01:40:12Z)\n"), "summary's first line");
        CHECK(plain->find("interval p50 16.7 / p95 16.7 / p99 16.7 ms, worst 64 ms") != std::string::npos, "summary's frame line");
        CHECK(plain->find("waits so far: regmem 3.3 s, throttle 1.2 s") != std::string::npos, "summary's waits");
    }

    // Too few frames and nothing measured: still well formed.
    perf::Snapshot empty;
    empty.generated = "2026-10-31T01:40:12Z";
    empty.presentedNs = { 1, 2, 3 };
    std::optional<std::string> bare = perf::Build(empty, in, refused);
    CHECK(bare, "empty refused: %s", refused.c_str());
    if (bare)
        CheckDocument(*bare);

    // 100 intervals: an estimate, with a note.
    perf::Snapshot few = Plain();
    few.presentedNs.resize(101);
    if (auto text = perf::Build(few, in, refused))
    {
        std::shared_ptr<Value> doc;
        CheckDocument(*text, &doc);
        const Value* fps = doc ? doc->Get("frames")->Get("fps_mean") : nullptr;
        CHECK(fps && std::get<std::string>(fps->Get("status")->v) == "estimated" && fps->Get("note"), "100 intervals are an estimate");
    }

    // The poisoned snapshot: the scrub leaves nothing for the denylist.
    std::optional<std::string> poisoned = perf::Build(Poisoned(), in, refused);
    CHECK(poisoned, "poisoned report refused: %s", refused.c_str());
    if (poisoned)
    {
        CheckDocument(*poisoned);
        for (const char* bad : { "/Users/", "~/", "/private/", "alice", "Alice", "Example", "MacBook", "0x82", "sub_82", "f.csv" })
            CHECK(poisoned->find(bad) == std::string::npos, "%s in the report", bad);
        CHECK(poisoned->find("\"RECARBON_FRAME_LOG\": \"(set)\"") != std::string::npos, "a path variable reads (set)");
        CHECK(poisoned->find("Apple M5") != std::string::npos, "the chip stays");
    }

    // What the scrub doesn't rewrite fails closed: nothing to write, and why.
    perf::Snapshot path = Plain();
    path.gpu = "Apple M5 (/Applications/ReCarbon.app)";
    CHECK(!perf::Build(path, in, refused) && refused.starts_with("a path"), "a path in a field: %s", refused.c_str());
    perf::Snapshot save = Plain();
    save.threads.push_back({ "slot0.sav", 1 });
    CHECK(!perf::Build(save, in, refused) && !refused.empty(), "a save name in a field");
    perf::Snapshot settingsFile = Plain();
    settingsFile.frameRateNote = "from settings.toml";
    CHECK(!perf::Build(settingsFile, in, refused) && !refused.empty(), "settings.toml in a field");

    // Strings that would break the JSON are escaped.
    perf::Snapshot quotes = Plain();
    quotes.gpu = "a \"quoted\" \\ name\twith\ncontrol\x01";
    if (auto text = perf::Build(quotes, in, refused))
        CheckDocument(*text);
    else
        CHECK(false, "quotes refused: %s", refused.c_str());
}

static void TestRing()
{
    printf("present ring\n");
    video::PresentRing<8> ring;
    CHECK(ring.Copy().empty(), "empty");
    for (int i = 1; i <= 5; i++)
        ring.Push(i);
    CHECK(ring.Copy() == std::vector<int64_t>({ 1, 2, 3, 4, 5 }), "under capacity");
    for (int i = 6; i <= 20; i++)
        ring.Push(i);
    // 20 pushed into 8: the newest 7 (the oldest slot is the next to be written).
    CHECK(ring.Count() == 20 && ring.Copy() == std::vector<int64_t>({ 14, 15, 16, 17, 18, 19, 20 }), "wrapped: %zu", ring.Copy().size());

    // A copy while the writer runs is always increasing and consecutive.
    auto big = std::make_unique<video::PresentRing<1024>>();
    std::atomic<bool> stop{ false };
    std::thread writer([&] { for (int64_t i = 1; !stop.load(std::memory_order_relaxed); i++) big->Push(i); });
    bool ordered = true;
    for (int k = 0; k < 2000 && ordered; k++)
    {
        std::vector<int64_t> c = big->Copy();
        for (size_t i = 1; i < c.size(); i++)
            ordered &= c[i] == c[i - 1] + 1;
    }
    stop = true;
    writer.join();
    CHECK(ordered, "a copy under a running writer has a torn entry");

    // Push's cost (printed, not checked).
    auto ring16k = std::make_unique<video::PresentRing<16384>>();
    constexpr int kPushes = 10'000'000;
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < kPushes; i++)
        ring16k->Push(i);
    double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count() / kPushes;
    auto copyStart = std::chrono::steady_clock::now();
    std::vector<int64_t> copy = ring16k->Copy();
    double copyUs = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - copyStart).count();
    auto statsStart = std::chrono::steady_clock::now();
    perf::IntervalStats iv = perf::Intervals(copy);
    double statsUs = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - statsStart).count();
    printf("  Push: %.2f ns each (%d pushes); Copy of %zu: %.0f us; percentiles: %.0f us (n %zu)\n", ns, kPushes, copy.size(), copyUs,
        statsUs, iv.n);
}

int main(int argc, char** argv)
{
    TestQuantiles();
    TestEnvPolicy();
    TestThreadLabels();
    TestWarnings();
    TestScrub();
    TestReports();
    TestRing();
    if (argc > 1)
    {
        std::string refused;
        std::ofstream(argv[1]) << perf::Build(Poisoned(), AliceInputs(), refused).value_or("refused: " + refused);
    }
    printf(g_failures ? "%d FAILED\n" : "all passed\n", g_failures);
    return g_failures ? 1 : 0;
}

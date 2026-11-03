// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// Export Performance Report (Settings > Advanced; report/perf_report.cpp
// gathers it): one text file a tester attaches to an issue, so reports from
// different Macs compare. The format (reference/PERF_REPORT_FORMAT.md):
//
//   a plain-text summary (the lines to paste)
//   --- machine-readable (recarbon-perf-report/1) ---
//   ```json
//   { "schema": "recarbon-perf-report/1", ... }
//   ```
//
// Every metric in the JSON is an object with a "status": "measured" (read
// from a counter or clock that measures it), "estimated" (derived, or from
// a shorter window than its name says), or "unavailable" ("value" is null
// and "why" says what is missing). Header only: the snapshot is plain data,
// so tests/perf_report_test.cpp formats, scrubs and checks it without the
// runtime.
#pragma once
#include "perf_scrub.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace report::perf
{
    inline constexpr const char* kSchema = "recarbon-perf-report/1";

    enum class Status : uint8_t { Measured, Estimated, Unavailable };
    inline const char* StatusName(Status s)
    {
        return s == Status::Measured ? "measured" : s == Status::Estimated ? "estimated" : "unavailable";
    }

    // --- Frame intervals ---

    // The index of quantile `p` (0-100) of `n` sorted values:
    // sorted[min(n-1, int(p/100*(n-1)+0.5))] (the nearest rank, rounded;
    // scripts/frame_times.py's int(p/100*n) can be one rank higher).
    inline size_t QuantileIndex(size_t n, double p)
    {
        return std::min(n - 1, size_t(p / 100.0 * double(n - 1) + 0.5));
    }
    inline double Quantile(const std::vector<double>& sorted, double p)
    {
        return sorted.empty() ? NAN : sorted[QuantileIndex(sorted.size(), p)];
    }

    // Intervals, how far to trust them: 10 s at 60 fps are a measurement;
    // one second to ten, an estimate; less, nothing to go on.
    inline constexpr size_t kMeasuredIntervals = 600, kEstimatedIntervals = 60;
    inline Status FrameStatus(size_t intervals)
    {
        return intervals >= kMeasuredIntervals ? Status::Measured : intervals >= kEstimatedIntervals ? Status::Estimated : Status::Unavailable;
    }

    struct IntervalStats
    {
        size_t n = 0;  // intervals
        double p50 = NAN, p95 = NAN, p99 = NAN, maxMs = NAN, meanFps = NAN, durationS = 0;
        uint64_t over25 = 0, over33 = 0, over50 = 0;  // > 25, > 33.4, > 50 ms
        Status status = Status::Unavailable;
    };

    // From consecutive present times (ns, steady clock) of new game frames.
    // No clamp: a 749 ms frame (a Space switch) stays the worst.
    inline IntervalStats Intervals(const std::vector<int64_t>& presentedNs)
    {
        IntervalStats s;
        std::vector<double> ms;
        for (size_t i = 1; i < presentedNs.size(); i++)
            ms.push_back(double(presentedNs[i] - presentedNs[i - 1]) / 1e6);
        s.n = ms.size();
        s.status = FrameStatus(s.n);
        if (ms.empty())
            return s;
        double sum = 0;
        for (double v : ms)
        {
            sum += v;
            s.over25 += v > 25.0;
            s.over33 += v > 33.4;
            s.over50 += v > 50.0;
        }
        std::sort(ms.begin(), ms.end());
        s.p50 = Quantile(ms, 50);
        s.p95 = Quantile(ms, 95);
        s.p99 = Quantile(ms, 99);
        s.maxMs = ms.back();
        s.durationS = sum / 1000.0;
        s.meanFps = sum > 0 ? double(s.n) * 1000.0 / sum : NAN;
        return s;
    }

    // --- Environment switches ---

    // Never valued, whatever else says: they carry paths, URLs or scripts.
    inline constexpr std::string_view kNeverValued[] = {
        "RECARBON_FRAME_LOG", "RECARBON_VBLANK_LOG", "RECARBON_BUG_REPORT_DIR", "RECARBON_PERF_REPORT_DIR", "RECARBON_GAME_DIR",
        "RECARBON_SESSION_LOG", "RECARBON_UPDATE_URL", "RECARBON_UPDATE_REPOSITORY", "RECARBON_UPDATE_CURL", "RECARBON_AUDIO_DUMP",
        "RECARBON_DUMP_FRAMES", "RECARBON_DUMP_SHADERS", "RECARBON_MOLTENVK", "RECARBON_INPUT_SCRIPT", "RECARBON_UI_KEYS",
        "RECARBON_LIFELINE_PID", "RECARBON_TRACE",
    };
    // The experimental and tuning switches worth comparing across reports
    // (docs/performance.md, the source's own comments), with their values.
    // The Settings options' own variables are allowed too (the caller passes
    // them). Any other RECARBON_* variable that is set reads "(set)".
    inline constexpr std::string_view kValued[] = {
        "RECARBON_GPU_WAIT_BACKOFF", "RECARBON_GPU_WAIT_BACKOFF_AT", "RECARBON_SLEEP0_BACKOFF", "RECARBON_SLEEP0_BACKOFF_AT",
        "RECARBON_SLEEP0_BACKOFF_FRAME_THREADS", "RECARBON_AA_DOWNSCALE_GATE", "RECARBON_SELECT_MUL", "RECARBON_DYNAMIC_SCALE",
        "RECARBON_PASS_PROFILE", "RECARBON_PASS_PROFILE_SPLIT", "RECARBON_FRAME_THREAD_QOS", "RECARBON_FRAME_THREAD_QOS_AT",
        "RECARBON_RENDER_SCALE", "RECARBON_THERMAL_SIM", "RECARBON_HITCH_MS", "RECARBON_HITCH_LATE_MS", "RECARBON_PRESENT_TIMING",
        "RECARBON_PRESENT_AT", "RECARBON_PRESENT_HOLD", "RECARBON_PRESENT_SCALE", "RECARBON_GPU_TIMING", "RECARBON_VERTEX_COPY_128",
        "RECARBON_VBLANK_LOCK", "RECARBON_VBLANK_HZ", "RECARBON_VBLANK_TIMER", "RECARBON_REFRESH_MULTIPLE", "RECARBON_CP_OPT",
        "RECARBON_CP_OWN_CORE", "RECARBON_EAGER_WRITES", "RECARBON_ASYNC_PIPELINES", "RECARBON_PIPELINE_THREADS",
        "RECARBON_PIPELINE_WAIT_US", "RECARBON_WATCH_SUBPAGES", "RECARBON_SCALED_FUSE", "RECARBON_SCALED_WANTED",
        "RECARBON_SCALED_DIRECT", "RECARBON_PASS_MERGE", "RECARBON_PASS_BARRIER", "RECARBON_OCCLUSION", "RECARBON_SHADING_RATE",
        "RECARBON_TILING", "RECARBON_TEXTURE_CACHE", "RECARBON_SHADER_CACHE", "RECARBON_SHADOW_MEMORY", "RECARBON_SHARED_MEMORY",
        "RECARBON_FAST_SCALE", "RECARBON_MOVIE_CADENCE", "RECARBON_HANG_SECS", "RECARBON_HEADLESS", "RECARBON_MUTE",
    };

    // A value that can only be a number, a word or a list of them: no "/",
    // no spaces, 40 characters at most.
    inline bool SimpleValue(std::string_view v)
    {
        if (v.empty() || v.size() > 40)
            return false;
        for (char c : v)
            if (!(std::isalnum(uint8_t(c)) || c == '.' || c == ',' || c == ':' || c == '+' || c == '-' || c == '_'))
                return false;
        return true;
    }

    // What the report says of RECARBON_<name>=<value>.
    inline std::string EnvValue(std::string_view name, std::string_view value, std::span<const std::string_view> settingsVars = {})
    {
        for (std::string_view never : kNeverValued)
            if (name == never)
                return "(set)";
        bool allowed = std::find(std::begin(kValued), std::end(kValued), name) != std::end(kValued)
            || std::find(settingsVars.begin(), settingsVars.end(), name) != settingsVars.end();
        return allowed && SimpleValue(value) ? std::string(value) : "(set)";
    }

    // --- Thread names ---

    // How a host thread is named in the report: the runtime's own names
    // ("recarbon-cp") as they are; a guest thread, "g<id>-<entry address>",
    // as "guest-<id>" (no address); a name the game gave a thread kept when
    // it is a plain word or two (no run of 6 hex digits, 31 characters at
    // most); anything else "other"; none "(unnamed)".
    inline std::string ThreadLabel(std::string_view name)
    {
        if (name.empty())
            return "(unnamed)";
        if (name.size() > 2 && name[0] == 'g' && std::isdigit(uint8_t(name[1])))
        {
            size_t dash = name.find('-');
            if (dash != std::string_view::npos && name.size() - dash - 1 == 8
                && std::all_of(name.begin() + 1, name.begin() + ptrdiff_t(dash), [](char c) { return std::isdigit(uint8_t(c)); })
                && std::all_of(name.begin() + ptrdiff_t(dash) + 1, name.end(), [](char c) { return std::isxdigit(uint8_t(c)); }))
                return "guest-" + std::string(name.substr(1, dash - 1));
        }
        if (name.size() > 31)
            return "other";
        size_t hexRun = 0;
        for (char c : name)
        {
            if (!(std::isalnum(uint8_t(c)) || c == ' ' || c == '_' || c == '-' || c == '.'))
                return "other";
            hexRun = std::isxdigit(uint8_t(c)) ? hexRun + 1 : 0;
            if (hexRun >= 6)
                return "other";
        }
        return std::string(name);
    }

    // --- Warnings in the log ---

    // Lines of the session log's in-memory tail that warn or fail, counted
    // by their "[tag]" (the line itself never goes in the report).
    inline std::vector<std::pair<std::string, uint64_t>> CountWarnings(std::string_view log, size_t* lines = nullptr)
    {
        std::vector<std::pair<std::string, uint64_t>> counts;
        size_t scanned = 0;
        for (size_t from = 0; from < log.size();)
        {
            size_t end = log.find('\n', from);
            if (end == std::string_view::npos)
                end = log.size();
            std::string_view line = log.substr(from, end - from);
            from = end + 1;
            if (line.empty())
                continue;
            scanned++;
            std::string lower(line);
            for (char& c : lower)
                c = char(std::tolower(uint8_t(c)));
            bool warns = false;
            for (std::string_view word : { "warning", "error", "failed", "couldn't", "ignored", "lost" })
                warns |= lower.find(word) != std::string::npos;
            if (!warns)
                continue;
            std::string tag = "(untagged)";
            if (line.size() > 2 && line[0] == '[')
                if (size_t close = line.find(']'); close != std::string_view::npos && close <= 25)
                {
                    std::string_view t = line.substr(1, close - 1);
                    if (!t.empty() && std::all_of(t.begin(), t.end(), [](char c) { return std::islower(uint8_t(c)) || std::isdigit(uint8_t(c)) || c == ' ' || c == '-' || c == '_'; }))
                        tag = std::string(t);
                }
            auto it = std::find_if(counts.begin(), counts.end(), [&](const auto& p) { return p.first == tag; });
            if (it == counts.end())
                counts.emplace_back(tag, 1);
            else
                it->second++;
        }
        std::sort(counts.begin(), counts.end(), [](const auto& a, const auto& b) { return a.second != b.second ? a.second > b.second : a.first < b.first; });
        if (lines)
            *lines = scanned;
        return counts;
    }

    // --- The snapshot ---

    // The command processor's waits, in renderer::CpWait's order.
    inline constexpr const char* kWaitKinds[] = { "regmem", "throttle", "shadow", "flush", "slot", "pipeline", "suspended" };
    inline constexpr size_t kWaitKindCount = std::size(kWaitKinds);

    // macOS's thermal pressure level (com.apple.system.thermalpressurelevel).
    inline const char* PressureName(uint64_t level)
    {
        switch (level)
        {
        case 0: return "nominal";
        case 1: return "moderate";
        case 2: return "heavy";
        case 3: return "trapping";
        case 4: return "sleeping";
        default: return "unknown";
        }
    }

    struct Snapshot
    {
        std::string generated;   // "2026-10-31T01:40:12Z"
        std::string trigger;     // "settings-row" or "ui-key"
        std::string version, commit, build;
        double sessionS = 0;
        // This computer.
        std::string machine, chip, os, gpu;
        int perfCores = -1, effCores = -1;
        double memoryGb = -1;
        // The display (presenter).
        bool displayKnown = false;
        int modeW = 0, modeH = 0, windowW = 0, windowH = 0, swapW = 0, swapH = 0;
        double hz = 0;
        bool fullscreen = false, vsync = false, presentsTimed = false;
        // Settings and what they gave.
        std::string resolutionSetting, aaSetting, frameRateSetting, vsyncSetting;
        int frameRateNow = 0;
        std::string frameRateNote;
        bool appliedKnown = false, appliedFixedByEnv = false, appliedDynamic = false;
        int cappedState = -1;  // the 4K cap (video/display_cap.h): 1 capped, 0 not, -1 not known
        std::string appliedScale;  // "2x", "1.5x"
        int appliedW = 0, appliedH = 0;
        // New game frames' present times (the present ring), and how many in all.
        std::vector<int64_t> presentedNs;
        uint64_t framesPresented = 0;
        // Present stats (presenter): since launch.
        uint64_t dropped = 0, repeated = 0, shown = 0;
        // The command processor's last [perf] tick (120 swaps), and its totals.
        bool tickKnown = false;
        double guestFps = 0, worstMs = 0, cpBusyMs = 0, draws = 0, gpuBusyMs = 0, gpuExecMs = 0;
        bool gpuTimestamps = false;
        uint64_t swaps = 0, lateFrames = 0;
        double cpBusyTotalMs = 0, waitTotalMs[kWaitKindCount] = {};
        double postMs = -1;  // the presenter's GPU time per frame (RECARBON_GPU_TIMING); < 0: not measured
        double d3dWaitTotalMs = -1;  // the game's Direct3D GPU waits, session total (timeline::g_gameWaitNs); < 0: not read
        uint64_t tiledDrawDrops = 0;  // predicated draws dropped by the bin test, session total
        bool tiledKnown = false;
        // Thermal.
        std::string thermalNow;  // "" or "unknown": no reading
        bool thermalSimulated = false;
        std::vector<std::pair<double, std::string>> thermalHistory;  // (seconds, state)
        bool pressureKnown = false;
        uint64_t pressureLevel = 0;
        // CPU time.
        bool cpuKnown = false;
        double userMs = 0, systemMs = 0;
        bool threadsKnown = false;
        std::vector<std::pair<std::string, double>> threads;  // (runtime name, ms)
        // Warnings in the log's tail.
        bool logKnown = false;
        size_t logLines = 0;
        std::vector<std::pair<std::string, uint64_t>> warnings;
        // Switches: RECARBON_* (EnvValue), settings not at their default, and
        // those an environment variable overrides.
        std::vector<std::pair<std::string, std::string>> env, settings;
        std::vector<std::string> overridden;
    };

    // --- JSON ---

    inline std::string Str(std::string_view s)
    {
        std::string out = "\"";
        for (char c : s)
        {
            if (c == '"' || c == '\\')
                out += '\\', out += c;
            else if (c == '\n')
                out += "\\n";
            else if (c == '\t')
                out += "\\t";
            else if (uint8_t(c) < 0x20)
                out += std::format("\\u{:04x}", unsigned(uint8_t(c)));
            else
                out += c;
        }
        return out + "\"";
    }
    inline std::string Num(double v, int decimals = 1)
    {
        if (!std::isfinite(v))
            return "null";
        std::string s = std::format("{:.{}f}", v, decimals);
        return s[0] == '-' && s.find_first_not_of("-0.") == std::string::npos ? s.substr(1) : s;  // no "-0.0"
    }
    inline std::string Int(uint64_t v) { return std::to_string(v); }
    inline std::string Bool(bool v) { return v ? "true" : "false"; }

    using Fields = std::vector<std::pair<std::string, std::string>>;
    inline std::string Obj(const Fields& fields, int indent = -1)
    {
        if (fields.empty())
            return "{}";
        const std::string pad = indent < 0 ? "" : "\n" + std::string(size_t(indent + 2), ' ');
        std::string out = "{";
        for (size_t i = 0; i < fields.size(); i++)
            out += (i ? "," : "") + (indent < 0 ? std::string(" ") : pad) + Str(fields[i].first) + ": " + fields[i].second;
        return out + (indent < 0 ? " }" : "\n" + std::string(size_t(indent), ' ') + "}");
    }

    // A metric: { "value": ..., "status": ..., "source": ..., (more) }; an
    // unavailable one has a null value and says why.
    inline std::string Metric(std::string value, Status status, std::string_view source, Fields more = {})
    {
        Fields f{ { "value", status == Status::Unavailable ? "null" : std::move(value) }, { "status", Str(StatusName(status)) },
            { "source", Str(source) } };
        f.insert(f.end(), more.begin(), more.end());
        return Obj(f);
    }
    inline std::string Unavailable(std::string_view source, std::string_view why)
    {
        return Metric("null", Status::Unavailable, source, { { "why", Str(why) } });
    }

    inline std::string Json(const Snapshot& s)
    {
        const Status M = Status::Measured;
        const IntervalStats iv = Intervals(s.presentedNs);
        Fields doc;
        doc.push_back({ "schema", Str(kSchema) });
        doc.push_back({ "generated", Str(s.generated) });
        doc.push_back({ "trigger", Str(s.trigger) });
        doc.push_back({ "session_s", Num(s.sessionS) });
        doc.push_back({ "build", Obj({ { "version", Str(s.version) }, { "commit", Str(s.commit) }, { "built", Str(s.build) } }) });

        auto text = [](const std::string& v, std::string_view source) {
            return v.empty() ? Unavailable(source, "not reported by this system") : Metric(Str(v), Status::Measured, source);
        };
        Fields system{ { "machine", text(s.machine, "sysctl hw.model") }, { "chip", text(s.chip, "sysctl machdep.cpu.brand_string") },
            { "cores", s.perfCores > 0 ? Metric(Obj({ { "performance", Int(uint64_t(s.perfCores)) }, { "efficiency", Int(uint64_t(std::max(s.effCores, 0))) } }), M, "sysctl hw.perflevel*.physicalcpu")
                                       : Unavailable("sysctl hw.perflevel*.physicalcpu", "no performance levels on this system") },
            { "memory_gb", s.memoryGb > 0 ? Metric(Num(s.memoryGb), M, "sysctl hw.memsize", { { "unit", Str("GB") } }) : Unavailable("sysctl hw.memsize", "not reported") },
            { "os", text(s.os, "sysctl kern.osproductversion") }, { "gpu", text(s.gpu, "Vulkan device properties") } };
        doc.push_back({ "system", Obj(system, 2) });

        Fields display;
        if (s.displayKnown)
        {
            display.push_back({ "mode", Metric(Obj({ { "width", Int(uint64_t(s.modeW)) }, { "height", Int(uint64_t(s.modeH)) }, { "hz", Num(s.hz, 2) } }), M, "SDL display mode") });
            display.push_back({ "window_px", Metric(Obj({ { "width", Int(uint64_t(s.windowW)) }, { "height", Int(uint64_t(s.windowH)) }, { "fullscreen", Bool(s.fullscreen) } }), M, "SDL window") });
            display.push_back({ "swapchain", Metric(Obj({ { "width", Int(uint64_t(s.swapW)) }, { "height", Int(uint64_t(s.swapH)) } }), M, "presenter") });
            display.push_back({ "vsync", Metric(Bool(s.vsync), M, "presenter (applied)", { { "setting", Str(s.vsyncSetting) } }) });
        }
        else
            display.push_back({ "mode", Unavailable("presenter", "no window yet") });
        display.push_back({ "present_timing", Metric(Bool(s.presentsTimed), M, "VK_KHR_present_wait") });
        display.push_back({ "frame_rate", Metric(Obj({ { "setting", Str(s.frameRateSetting) }, { "now", Int(uint64_t(s.frameRateNow)) },
                                                     { "note", s.frameRateNote.empty() ? "null" : Str(s.frameRateNote) } }), M, "video::FrameRate") });
        doc.push_back({ "display", Obj(display, 2) });

        Fields resolution{ { "setting", Metric(Str(s.resolutionSetting), M, "settings video.internal_resolution") },
            { "applied", s.appliedKnown ? Metric(Obj({ { "scale", Str(s.appliedScale) }, { "width", Int(uint64_t(s.appliedW)) }, { "height", Int(uint64_t(s.appliedH)) },
                                                       { "fixed_by_env", Bool(s.appliedFixedByEnv) }, { "dynamic", Bool(s.appliedDynamic) } }), M, "renderer (perf tick)")
                                        : Unavailable("renderer (perf tick)", "no frame drawn yet") },
            { "capped", s.cappedState >= 0 ? Metric(Bool(s.cappedState == 1), M, "renderer InternalResolutionScale (the 4K cap)")
                                           : Unavailable("renderer", "the scale isn't known yet (Auto before there is a picture)") },
            { "anti_aliasing", Metric(Str(s.aaSetting), M, "settings video.anti_aliasing") } };
        doc.push_back({ "resolution", Obj(resolution, 2) });

        Fields frames{ { "window", Str("last presents in the ring (host steady clock)") },
            { "frames_presented", Metric(Int(s.framesPresented), M, "present-ring") },
            { "intervals", Metric(Int(iv.n), M, "present-ring", { { "note", Str("new game frames only, up to the last 16384") } }) } };
        if (iv.status == Status::Unavailable)
        {
            const std::string why = std::format("{} intervals: at least {} are needed", iv.n, kEstimatedIntervals);
            for (const char* k : { "duration_s", "fps_mean", "interval_ms", "over_25ms", "over_33ms", "over_50ms" })
                frames.push_back({ k, Unavailable("present-ring", why) });
        }
        else
        {
            Fields note;
            if (iv.status == Status::Estimated)
                note.push_back({ "note", Str(std::format("only {} intervals (measured from {})", iv.n, kMeasuredIntervals)) });
            auto with = [&](Fields f) { f.insert(f.end(), note.begin(), note.end()); return f; };
            frames.push_back({ "duration_s", Metric(Num(iv.durationS), iv.status, "present-ring", with({ { "unit", Str("s") } })) });
            frames.push_back({ "fps_mean", Metric(Num(iv.meanFps), iv.status, "present-ring", with({ { "unit", Str("fps") } })) });
            frames.push_back({ "interval_ms", Metric(Obj({ { "p50", Num(iv.p50, 2) }, { "p95", Num(iv.p95, 2) }, { "p99", Num(iv.p99, 2) }, { "max", Num(iv.maxMs, 2) } }),
                                   iv.status, "present-ring", with({ { "unit", Str("ms") }, { "quantile", Str("sorted[min(n-1, int(p/100*(n-1)+0.5))]") } })) });
            frames.push_back({ "over_25ms", Metric(Int(iv.over25), iv.status, "present-ring", with({ { "threshold_ms", "25" } })) });
            frames.push_back({ "over_33ms", Metric(Int(iv.over33), iv.status, "present-ring", with({ { "threshold_ms", "33.4" } })) });
            frames.push_back({ "over_50ms", Metric(Int(iv.over50), iv.status, "present-ring", with({ { "threshold_ms", "50" } })) });
        }
        doc.push_back({ "frames", Obj(frames, 2) });

        Fields events{ { "window", Str("session") }, { "dropped", Metric(Int(s.dropped), M, "present-stats") }, { "shown", Metric(Int(s.shown), M, "present-stats") },
            { "repeated_refreshes", s.presentsTimed ? Metric(Int(s.repeated), M, "present-stats")
                                                    : Unavailable("present-stats", "presents aren't timed (no VK_KHR_present_wait)") } };
        doc.push_back({ "display_events", Obj(events, 2) });

        Fields cp, gpu, hitches;
        if (s.tickKnown)
        {
            Fields waits;
            for (size_t k = 0; k < kWaitKindCount; k++)
                waits.push_back({ kWaitKinds[k], Num(s.waitTotalMs[k]) });
            cp = { { "window", Str("per frame: the last 120 swaps; totals: session") },
                { "guest_fps", Metric(Num(s.guestFps), M, "perf-tick", { { "unit", Str("fps") }, { "note", Str("game clock, last 120 swaps") } }) },
                { "worst_frame_ms", Metric(Num(s.worstMs), M, "perf-tick", { { "unit", Str("ms") } }) },
                { "busy_ms_per_frame", Metric(Num(s.cpBusyMs, 2), M, "perf-tick", { { "unit", Str("ms") } }) },
                { "draws_per_frame", Metric(Num(s.draws, 0), M, "perf-tick") },
                { "busy_ms_total", Metric(Num(s.cpBusyTotalMs), M, "perf-tick cumulative", { { "unit", Str("ms") } }) },
                { "wait_ms_total", Metric(Obj(waits), M, "perf-tick cumulative", { { "unit", Str("ms") } }) } };
            gpu = { { "busy_ms_per_frame", Metric(Num(s.gpuBusyMs, 2), M, "perf-tick", { { "unit", Str("ms") } }) },
                { "executing_ms_per_frame", s.gpuTimestamps ? Metric(Num(s.gpuExecMs, 2), M, "perf-tick (GPU timestamps)", { { "unit", Str("ms") } })
                                                            : Unavailable("perf-tick", "no GPU timestamps on this device") } };
            hitches = { { "swaps", Metric(Int(s.swaps), M, "cp-stats") },
                { "late_frames", Metric(Int(s.lateFrames), M, "cp-stats", { { "rule", Str("swap-to-swap game time >= 50 ms, or > the median of the last 63 + 3 ms (RECARBON_HITCH_MS, RECARBON_HITCH_LATE_MS)") } }) } };
        }
        else
        {
            cp = { { "busy_ms_per_frame", Unavailable("perf-tick", "fewer than 120 frames drawn") } };
            gpu = { { "busy_ms_per_frame", Unavailable("perf-tick", "fewer than 120 frames drawn") } };
            hitches = { { "late_frames", Unavailable("cp-stats", "fewer than 120 frames drawn") } };
        }
        gpu.push_back({ "post_processing_ms", s.postMs >= 0 ? Metric(Num(s.postMs, 2), M, "presenter (RECARBON_GPU_TIMING)", { { "unit", Str("ms") } })
                                                            : Unavailable("presenter", "RECARBON_GPU_TIMING is not set") });
        hitches.push_back({ "d3d_wait", s.d3dWaitTotalMs >= 0 ? Metric(Num(s.d3dWaitTotalMs), M, "gpu-waits cumulative", { { "unit", Str("ms") } })
                                                            : Unavailable("gpu-waits", "not read") });
        hitches.push_back({ "world_step_cadence", Unavailable("world", "only with RECARBON_GAME_TIME_LOG (its CSV and [gametime] lines)") });
        hitches.push_back({ "tiled_draw_drops", s.tiledKnown ? Metric(Int(s.tiledDrawDrops), M, "command processor cumulative")
                                                             : Unavailable("command processor", "not read") });
        doc.push_back({ "cp", Obj(cp, 2) });
        doc.push_back({ "gpu", Obj(gpu, 2) });
        doc.push_back({ "hitches", Obj(hitches, 2) });

        Fields thermal;
        const bool thermalKnown = !s.thermalNow.empty() && s.thermalNow != "unknown";
        thermal.push_back({ "state_now", thermalKnown ? Metric(Str(s.thermalNow), M, "NSProcessInfo thermalState", { { "simulated", Bool(s.thermalSimulated) } })
                                                      : Unavailable("NSProcessInfo thermalState", "no reading on this system") });
        std::string history = "[";
        for (size_t i = 0; i < s.thermalHistory.size(); i++)
            history += (i ? ", " : "") + Obj({ { "at_s", Num(s.thermalHistory[i].first) }, { "state", Str(s.thermalHistory[i].second) } });
        history += "]";
        thermal.push_back({ "history", thermalKnown ? Metric(history, M, "thermal poll (changes, at most 32)") : Unavailable("thermal poll", "no reading on this system") });
        thermal.push_back({ "pressure_level", s.pressureKnown ? Metric(Int(s.pressureLevel), M, "notify com.apple.system.thermalpressurelevel", { { "name", Str(PressureName(s.pressureLevel)) } })
                                                              : Unavailable("notify com.apple.system.thermalpressurelevel", "notify_get_state failed") });
        doc.push_back({ "thermal", Obj(thermal, 2) });

        Fields cpu;
        cpu.push_back({ "process_ms", s.cpuKnown ? Metric(Obj({ { "user", Num(s.userMs, 0) }, { "system", Num(s.systemMs, 0) } }), M, "getrusage", { { "window", Str("session") } })
                                                 : Unavailable("getrusage", "failed") });
        std::string threads = "[";
        for (size_t i = 0; i < s.threads.size(); i++)
            threads += (i ? ", " : "") + Obj({ { "name", Str(s.threads[i].first) }, { "cpu_ms", Num(s.threads[i].second, 0) } });
        threads += "]";
        cpu.push_back({ "threads", s.threadsKnown ? Metric(threads, M, "thread_info (one sample at export)", { { "window", Str("session") } })
                                                  : Unavailable("thread_info", "not available on this system") });
        doc.push_back({ "cpu", Obj(cpu, 2) });

        Fields warnings;
        for (const auto& [tag, n] : s.warnings)
            warnings.push_back({ tag, Int(n) });
        doc.push_back({ "warnings", s.logKnown ? Metric(Obj(warnings), Status::Estimated, "log tail (lines that warn or fail, by tag)",
                                                     { { "lines_scanned", Int(s.logLines) } })
                                               : Unavailable("log tail", "no session log (RECARBON_SESSION_LOG=0)") });

        Fields env, settings;
        for (const auto& [k, v] : s.env)
            env.push_back({ k, Str(v) });
        for (const auto& [k, v] : s.settings)
            settings.push_back({ k, Str(v) });
        std::string overridden = "[";
        for (size_t i = 0; i < s.overridden.size(); i++)
            overridden += (i ? ", " : "") + Str(s.overridden[i]);
        overridden += "]";
        doc.push_back({ "flags", Obj({ { "env", Obj(env) }, { "settings_not_default", Obj(settings) }, { "overridden_by_env", overridden } }, 2) });
        return Obj(doc, 0);
    }

    inline std::string Fixed(double v, int decimals) { return std::isfinite(v) ? std::format("{:.{}f}", v, decimals) : "-"; }

    // The lines to paste into an issue.
    inline std::string Summary(const Snapshot& s)
    {
        const IntervalStats iv = Intervals(s.presentedNs);
        std::string t = std::format("ReCarbon performance report (v{} {}, {})\n", s.version, s.commit, s.generated);
        t += std::format("{}, {}{}, {} GB, {}\n", s.machine.empty() ? "unknown machine" : s.machine, s.chip.empty() ? "unknown chip" : s.chip,
            s.perfCores > 0 ? std::format(" ({}P+{}E)", s.perfCores, std::max(s.effCores, 0)) : "", s.memoryGb > 0 ? Fixed(s.memoryGb, 0) : "?",
            s.os.empty() ? "unknown OS" : s.os);
        if (s.displayKnown)
            t += std::format("Display {}x{} at {} Hz, window {}x{}{}, swapchain {}x{}, V-Sync {}, presents {}\n", s.modeW, s.modeH, Fixed(s.hz, 2),
                s.windowW, s.windowH, s.fullscreen ? " fullscreen" : "", s.swapW, s.swapH, s.vsync ? "on" : "off", s.presentsTimed ? "timed" : "not timed");
        t += std::format("Internal resolution: {} -> {}; AA {}; Frame Rate {} (now {})\n", s.resolutionSetting,
            s.appliedKnown ? std::format("{} ({}x{})", s.appliedScale, s.appliedW, s.appliedH) : "unknown", s.aaSetting, s.frameRateSetting, s.frameRateNow);
        if (iv.status == Status::Unavailable)
            t += std::format("Frames: too few to measure ({} intervals)\n", iv.n);
        else
            t += std::format("Frames ({} s, {} intervals{}): {} fps; interval p50 {} / p95 {} / p99 {} ms, worst {} ms\n"
                             "  over 25 ms: {}   over 33.4 ms: {}   over 50 ms: {}   late (hitch rule): {}\n",
                Fixed(iv.durationS, 0), iv.n, iv.status == Status::Estimated ? ", estimated" : "", Fixed(iv.meanFps, 1), Fixed(iv.p50, 1),
                Fixed(iv.p95, 1), Fixed(iv.p99, 1), Fixed(iv.maxMs, 0), iv.over25, iv.over33, iv.over50, s.tickKnown ? std::to_string(s.lateFrames) : "-");
        t += std::format("Display: {} dropped, {} repeated refreshes, {} shown\n", s.dropped, s.presentsTimed ? std::to_string(s.repeated) : "-", s.shown);
        if (s.tickKnown)
        {
            std::vector<std::pair<double, const char*>> waits;
            for (size_t k = 0; k < kWaitKindCount; k++)
                if (s.waitTotalMs[k] >= 50)
                    waits.push_back({ s.waitTotalMs[k], kWaitKinds[k] });
            std::sort(waits.rbegin(), waits.rend());
            std::string w;
            for (const auto& [ms, kind] : waits)
                w += std::format("{}{} {} s", w.empty() ? "" : ", ", kind, Fixed(ms / 1000.0, 1));
            t += std::format("CP busy {} ms/frame ({} draws); waits so far: {}\n", Fixed(s.cpBusyMs, 1), Fixed(s.draws, 0), w.empty() ? "none" : w);
            t += std::format("GPU busy {} ms/frame, executing {}; post-processing {}\n", Fixed(s.gpuBusyMs, 1),
                s.gpuTimestamps ? Fixed(s.gpuExecMs, 1) + " ms" : "not measured", s.postMs >= 0 ? Fixed(s.postMs, 2) + " ms" : "not measured");
        }
        t += std::format("Thermal: {}{}; pressure {}\n", s.thermalNow.empty() ? "unknown" : s.thermalNow, s.thermalSimulated ? " (simulated)" : "",
            s.pressureKnown ? PressureName(s.pressureLevel) : "unknown");
        std::string flags;
        for (const auto& [k, v] : s.env)
            flags += (flags.empty() ? "" : ", ") + k + "=" + v;
        t += "Switches: " + (flags.empty() ? std::string("none") : flags) + "\n";
        t += "Nothing was sent anywhere. Paths, names, saves and game memory are not in this file.\n";
        return t;
    }

    inline std::string Format(const Snapshot& s)
    {
        return Summary(s) + "\n--- machine-readable (" + kSchema + ") ---\n```json\n" + Json(s) + "\n```\n";
    }

    // The text to write, scrubbed and checked: nullopt (and `refused` says
    // why) when the denylist still finds something, so nothing is written.
    inline std::optional<std::string> Build(const Snapshot& s, const perfscrub::Inputs& in, std::string& refused)
    {
        std::string text = perfscrub::Scrub(Format(s), in);
        refused = perfscrub::FindForbidden(text, in);
        if (!refused.empty())
            return std::nullopt;
        return text;
    }
}

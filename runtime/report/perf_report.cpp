// ReCarbon runtime. GPL-3.0-or-later (see COPYING). See report.h and
// perf_report.h.
//
// Export Performance Report (Settings > Advanced, or PerfReport in
// RECARBON_UI_KEYS): reads the counters the game keeps anyway, at the moment
// the player asks, on a thread of its own; formats them (perf_report.h);
// scrubs the text and checks it against the denylist (perf_scrub.h), writing
// nothing if the check finds anything; and saves one .md file where the bug
// report goes (the Desktop, else the data folder; RECARBON_PERF_REPORT_DIR
// instead, for scripts). Nothing is sent anywhere.
#include <stdafx.h>
#include "report.h"
#include "internal.h"
#include "perf_report.h"

#include <cpu/guest_thread.h>
#include <debug/timeline.h>
#include <gpu/command_processor.h>
#include <gpu/renderer.h>
#include <platform/thermal.h>
#include <ui/ui.h>
#include <user/settings.h>
#include <video/frame_rate.h>
#include <video/presenter.h>

#include <map>

#include <pwd.h>
#include <sys/resource.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach/mach.h>
#include <pthread.h>
#endif

extern char** environ;

namespace report
{
    namespace
    {
        constexpr const char* kDirVariable = "RECARBON_PERF_REPORT_DIR";

        std::atomic<BugReportState> s_state{ BugReportState::Idle };
        std::mutex s_mutex;
        std::string s_message;             // under s_mutex
        std::filesystem::path s_lastPath;  // under s_mutex

        void SetMessage(std::string text)
        {
            std::lock_guard lock(s_mutex);
            s_message = std::move(text);
        }

        std::string UtcNow()
        {
            time_t now = time(nullptr);
            struct tm tm{};
            gmtime_r(&now, &tm);
            char text[32];
            strftime(text, sizeof(text), "%Y-%m-%dT%H:%M:%SZ", &tm);
            return text;
        }

        // The names the scrub and the denylist look for: the login, the
        // account's full name, the host name. Read only for that: none of
        // them goes into the report.
        perfscrub::Inputs PrivateInputs()
        {
            perfscrub::Inputs in;
            in.homes = detail::HomeFolders();
            if (const passwd* pw = getpwuid(getuid()))
            {
                if (pw->pw_name && *pw->pw_name)
                    in.whole.push_back(pw->pw_name);
                if (pw->pw_gecos && *pw->pw_gecos)
                    in.names.push_back(pw->pw_gecos);
            }
            if (const char* login = getlogin(); login && *login)
                in.whole.push_back(login);
            if (const char* user = std::getenv("USER"); user && *user)
                in.whole.push_back(user);
            char host[256] = {};
            if (gethostname(host, sizeof(host) - 1) == 0 && host[0])
            {
                std::string name = host;
                in.whole.push_back(name);
                if (size_t dot = name.find('.'); dot != std::string::npos && dot > 0)
                    in.whole.push_back(name.substr(0, dot));
            }
            return in;
        }

        // Each thread's CPU time, summed by its label (perf::ThreadLabel),
        // most first: one task_threads/thread_info pass at export.
        bool ThreadTimes(std::vector<std::pair<std::string, double>>& out)
        {
#ifdef __APPLE__
            thread_act_array_t threads = nullptr;
            mach_msg_type_number_t count = 0;
            if (task_threads(mach_task_self(), &threads, &count) != KERN_SUCCESS)
                return false;
            std::map<std::string, double> byName;
            for (mach_msg_type_number_t i = 0; i < count; i++)
            {
                thread_basic_info_data_t info{};
                mach_msg_type_number_t infoCount = THREAD_BASIC_INFO_COUNT;
                if (thread_info(threads[i], THREAD_BASIC_INFO, thread_info_t(&info), &infoCount) == KERN_SUCCESS)
                {
                    char name[64] = {};
                    if (pthread_t pt = pthread_from_mach_thread_np(threads[i]))
                        pthread_getname_np(pt, name, sizeof(name));
                    const double ms = double(info.user_time.seconds + info.system_time.seconds) * 1000.0
                        + double(info.user_time.microseconds + info.system_time.microseconds) / 1000.0;
                    byName[perf::ThreadLabel(name)] += ms;
                }
                mach_port_deallocate(mach_task_self(), threads[i]);
            }
            vm_deallocate(mach_task_self(), vm_address_t(threads), vm_size_t(count * sizeof(thread_act_t)));
            out.assign(byName.begin(), byName.end());
            std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
            if (out.size() > 24)
                out.resize(24);
            return true;
#else
            (void)out;
            return false;
#endif
        }

        perf::Snapshot Gather(PerfReportTrigger trigger)
        {
            using settings::Id;
            perf::Snapshot s;
            s.generated = UtcNow();
            s.trigger = trigger == PerfReportTrigger::UiKey ? "ui-key" : "settings-row";
            s.version = Version();
            s.commit = BuildId();
            s.build = BuildString();
            s.sessionS = SessionSeconds();

            const detail::HostFacts host = detail::ReadHostFacts();
            s.machine = host.machine;
            s.chip = host.chip;
            s.os = host.os;
            s.perfCores = host.perfCores;
            s.effCores = host.effCores;
            s.memoryGb = host.memoryGb;
            s.gpu = detail::SystemItem("GPU");

            const video::DisplayInfo display = video::GetDisplayInfo();
            s.displayKnown = display.known;
            s.modeW = display.modeW, s.modeH = display.modeH, s.hz = display.hz;
            s.windowW = display.windowW, s.windowH = display.windowH, s.fullscreen = display.fullscreen;
            s.swapW = int(display.swapW), s.swapH = int(display.swapH), s.vsync = display.vsync;
            s.presentsTimed = display.presentsTimed;

            auto setting = [](Id id) { return settings::FormatValue(id, settings::GetValue(id)); };
            s.resolutionSetting = setting(Id::InternalResolution);
            {
                gpu::RenderScale now, asked;
                if (gpu::renderer::InternalResolutionScale(int32_t(settings::GetValue(Id::InternalResolution)), now, &asked))
                    s.cappedState = now == asked ? 0 : 1;
            }
            s.aaSetting = setting(Id::AntiAliasing);
            s.frameRateSetting = setting(Id::FrameRate);
            s.vsyncSetting = setting(Id::VSync);
            s.frameRateNow = video::FrameRate();
            s.frameRateNote = video::AutoFrameRateNote();

            const gpu::renderer::PerfTotals totals = gpu::renderer::GetPerfTotals();
            if (totals.ticks > 0)
            {
                s.appliedKnown = true;
                s.appliedScale = totals.scale;
                s.appliedW = int(totals.scaledW), s.appliedH = int(totals.scaledH);
                s.appliedFixedByEnv = totals.scaleFixed, s.appliedDynamic = totals.scaleDynamic;
                s.tickKnown = true;
                s.guestFps = totals.guestFps, s.worstMs = totals.worstMs, s.cpBusyMs = totals.cpBusyMs, s.draws = totals.draws;
                s.gpuBusyMs = totals.gpuBusyMs;
                s.gpuTimestamps = totals.gpuExecMs >= 0;
                s.gpuExecMs = std::max(totals.gpuExecMs, 0.0);
                s.postMs = totals.postMs;
                s.swaps = totals.swaps, s.lateFrames = totals.lateFrames;
                s.cpBusyTotalMs = double(totals.cpBusyNs) / 1e6;
                s.d3dWaitTotalMs = double(timeline::g_gameWaitNs.load(std::memory_order_relaxed)) / 1e6;
                s.tiledDrawDrops = gpu::BinDroppedDraws(), s.tiledKnown = true;
                for (size_t k = 0; k < perf::kWaitKindCount; k++)
                    s.waitTotalMs[k] = double(totals.waitNs[k]) / 1e6;
            }

            s.presentedNs = video::RecentFramePresents();
            s.framesPresented = video::FramesPresented();
            const video::PresentStats shown = video::GetPresentStats();
            s.dropped = shown.dropped, s.repeated = shown.repeated, s.shown = shown.shown;

            s.thermalNow = platform::thermal::Name(platform::thermal::Current());
            for (const platform::thermal::Step& step : platform::thermal::History())
            {
                s.thermalHistory.push_back({ step.at, platform::thermal::Name(step.state) });
                s.thermalSimulated |= step.simulated;
            }
            s.pressureKnown = platform::thermal::PressureLevel(s.pressureLevel);

            if (rusage usage{}; getrusage(RUSAGE_SELF, &usage) == 0)
            {
                s.cpuKnown = true;
                s.userMs = double(usage.ru_utime.tv_sec) * 1000.0 + double(usage.ru_utime.tv_usec) / 1000.0;
                s.systemMs = double(usage.ru_stime.tv_sec) * 1000.0 + double(usage.ru_stime.tv_usec) / 1000.0;
            }
            s.threadsKnown = ThreadTimes(s.threads);

            s.logKnown = !SessionLogPath().empty();
            if (s.logKnown)
                s.warnings = perf::CountWarnings(RecentOutput(4000), &s.logLines);

            std::vector<std::string_view> settingsVars;
            for (const settings::Option& o : settings::Options())
            {
                if (o.env)
                    settingsVars.push_back(o.env);
                if (o.hidden)
                    continue;
                if (!settings::IsDefault(o.id))
                    s.settings.push_back({ std::format("{}.{}", o.table, o.key), settings::FormatValue(o.id, settings::GetStoredValue(o.id)) });
                if (settings::EnvOverride(o.id))
                    s.overridden.push_back(std::format("{}.{}", o.table, o.key));
            }
            for (char** e = environ; e && *e; e++)
                if (std::string_view v = *e; v.starts_with("RECARBON_"))
                {
                    const size_t eq = v.find('=');
                    const std::string_view name = v.substr(0, eq), value = eq == std::string_view::npos ? std::string_view() : v.substr(eq + 1);
                    s.env.push_back({ std::string(name), perf::EnvValue(name, value, settingsVars) });
                }
            std::sort(s.env.begin(), s.env.end());
            return s;
        }

        void Save(PerfReportTrigger trigger, std::string stamp)
        {
            SetHostThreadName("recarbon-perfreport");
            const auto started = std::chrono::steady_clock::now();
            std::string refused;
            const std::optional<std::string> text = perf::Build(Gather(trigger), PrivateInputs(), refused);
            if (!text)
            {
                // Fails closed: the check found something private (its kind
                // only, never the text, goes to the log and the toast).
                std::string why = "Performance report not saved: it would have contained " + refused + ". Nothing was written.";
                fprintf(stderr, "[perfreport] refused by the privacy check: %s\n", refused.c_str());
                SetMessage(why);
                s_state.store(BugReportState::Failed);
                ui::Toast(why, 12.0);
                return;
            }
            const std::string name = std::format("recarbon-perf-report-v{}-{}.md", Version(), stamp);
            detail::SavedFile saved = detail::SaveReportFile(kDirVariable, "perfreport", name, text->data(), text->size());
            if (!saved.error.empty())
            {
                std::string why = std::format("Couldn't save the performance report in {}: {}", ScrubHome(saved.folder.string()), saved.error);
                fprintf(stderr, "[perfreport] %s\n", why.c_str());
                SetMessage(why);
                s_state.store(BugReportState::Failed);
                ui::Toast(why, 12.0);
                return;
            }
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
            const std::string size = std::format("{} KB", (text->size() + 1023) >> 10);
            const std::string where = detail::DescribeReportPath(saved.path);
            fprintf(stderr, "[perfreport] saved %s (%s, %.0f ms)\n", where.c_str(), size.c_str(), ms);
            {
                std::lock_guard lock(s_mutex);
                s_message = "Saved to " + where;
                s_lastPath = saved.path;
            }
            s_state.store(BugReportState::Saved);
            ui::Toast(std::format("Performance report saved: {} ({}). Attach it, or paste its first lines into your issue.", where, size), 15.0);
        }
    }

    void SavePerfReport(PerfReportTrigger trigger)
    {
        BugReportState state = s_state.load();
        if (state == BugReportState::Saving || !s_state.compare_exchange_strong(state, BugReportState::Saving))
            return;
        SetMessage("Saving...");
        fprintf(stderr, "[perfreport] Export Performance Report (%s)\n", trigger == PerfReportTrigger::UiKey ? "RECARBON_UI_KEYS" : "Settings");
        std::thread(Save, trigger, detail::TimeStamp()).detach();
    }

    BugReportState GetPerfReportState()
    {
        return s_state.load();
    }

    std::string PerfReportMessage()
    {
        std::lock_guard lock(s_mutex);
        return s_message;
    }

    std::filesystem::path LastPerfReportPath()
    {
        std::lock_guard lock(s_mutex);
        return s_lastPath;
    }
}

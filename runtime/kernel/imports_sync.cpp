// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// Synchronisation exports on top of kernel/dispatcher. Signatures and
// return values follow Xenia's xboxkrnl_threading.cc.
#include <stdafx.h>
#include "dispatcher.h"
#include "function.h"
#include <cpu/guest_thread.h>
#include <cpu/guest_time.h>
#include <cpu/host_cpu.h>
#include "sleep0_backoff.h"
#include "sleep0_predict.h"
#include "thread_priority.h"
#include "sleep0_schedule.h"
#include "sleep0_trace.h"
#if defined(__APPLE__)
#include <TargetConditionals.h>
#include <pthread.h>
#include <pthread/qos.h>
#include <time.h>
#endif
#if defined(__APPLE__) && !TARGET_OS_IOS
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <sys/sysctl.h>
#endif

using namespace dispatcher;

namespace
{
    constexpr uint32_t STATUS_OBJECT_TYPE_MISMATCH = 0xC0000024;

    template<typename T>
    T* FromHandle(uint32_t handle)
    {
        if (handle == GUEST_INVALID_HANDLE_VALUE || !IsKernelObject(handle))
            return nullptr;
        return dynamic_cast<T*>(GetKernelObject(handle));
    }

    // Named objects (OBJECT_ATTRIBUTES with a name) aren't supported yet;
    // make their first use visible instead of silently creating a new one.
    void CheckUnnamed(const XOBJECT_ATTRIBUTES* attributes, const char* who)
    {
        if (attributes != nullptr && attributes->Name.get() != 0)
            fprintf(stderr, "[sync] %s: named objects are not supported yet (creating an unnamed one)\n", who);
    }

    uint32_t WaitHandles(std::span<const be<uint32_t>> handles, bool waitAll, const be<int64_t>* timeout)
    {
        std::vector<Waitable*> objects;
        for (uint32_t h : handles)
        {
            Waitable* w = WaitableFromHandle(h);
            if (w == nullptr)
                return STATUS_INVALID_HANDLE;
            objects.push_back(w);
        }
        return WaitForObjects(objects, waitAll, Deadline(timeout));
    }
}

// ---- Events ------------------------------------------------------------------

uint32_t NtCreateEvent(be<uint32_t>* handle, XOBJECT_ATTRIBUTES* attributes, uint32_t eventType, uint32_t initialState)
{
    CheckUnnamed(attributes, "NtCreateEvent");
    // eventType: 0 = NotificationEvent (manual reset), 1 = SynchronizationEvent.
    *handle = GetKernelHandle(CreateKernelObject<EventObject>(eventType == 0, initialState != 0));
    return STATUS_SUCCESS;
}

uint32_t KeSetEvent(XKEVENT* event, uint32_t increment, uint32_t wait)
{
    return static_cast<EventObject*>(BindDispatcherHeader(*event))->Set();
}

uint32_t KeResetEvent(XKEVENT* event)
{
    return static_cast<EventObject*>(BindDispatcherHeader(*event))->Reset();
}

uint32_t KePulseEvent(XKEVENT* event, uint32_t increment, uint32_t wait)
{
    return static_cast<EventObject*>(BindDispatcherHeader(*event))->Pulse();
}

uint32_t NtSetEvent(uint32_t handle, be<uint32_t>* previousState)
{
    auto* e = FromHandle<EventObject>(handle);
    if (e == nullptr)
        return STATUS_INVALID_HANDLE;
    bool previous = e->Set();
    if (previousState)
        *previousState = previous;
    return STATUS_SUCCESS;
}

uint32_t NtClearEvent(uint32_t handle)
{
    auto* e = FromHandle<EventObject>(handle);
    if (e == nullptr)
        return STATUS_INVALID_HANDLE;
    e->Reset();
    return STATUS_SUCCESS;
}

uint32_t NtPulseEvent(uint32_t handle, be<uint32_t>* previousState)
{
    auto* e = FromHandle<EventObject>(handle);
    if (e == nullptr)
        return STATUS_INVALID_HANDLE;
    bool previous = e->Pulse();
    if (previousState)
        *previousState = previous;
    return STATUS_SUCCESS;
}

// ---- Semaphores ----------------------------------------------------------------

void KeInitializeSemaphore(XKSEMAPHORE* semaphore, uint32_t count, uint32_t limit)
{
    // Guest-embedded: initialise the header; the host object binds on first use.
    semaphore->Header.Type = 5;
    semaphore->Header.SignalState = count;
    semaphore->Header.WaitListHead.Flink = 0;
    semaphore->Header.WaitListHead.Blink = 0;
    semaphore->Limit = limit;
}

uint32_t KeReleaseSemaphore(XKSEMAPHORE* semaphore, uint32_t increment, uint32_t adjustment, uint32_t wait)
{
    int32_t previous = 0;
    auto* s = static_cast<SemaphoreObject*>(BindDispatcherHeader(semaphore->Header));
    uint32_t status = s->Release(int32_t(adjustment), &previous);
    assert(status == STATUS_SUCCESS && "KeReleaseSemaphore past its limit");
    return uint32_t(previous);
}

uint32_t NtCreateSemaphore(be<uint32_t>* handle, XOBJECT_ATTRIBUTES* attributes, uint32_t count, uint32_t limit)
{
    CheckUnnamed(attributes, "NtCreateSemaphore");
    *handle = GetKernelHandle(CreateKernelObject<SemaphoreObject>(int32_t(count), int32_t(limit)));
    return STATUS_SUCCESS;
}

uint32_t NtReleaseSemaphore(uint32_t handle, uint32_t releaseCount, be<uint32_t>* previousCount)
{
    auto* s = FromHandle<SemaphoreObject>(handle);
    if (s == nullptr)
        return STATUS_INVALID_HANDLE;
    int32_t previous = 0;
    uint32_t status = s->Release(int32_t(releaseCount), &previous);
    if (previousCount)
        *previousCount = uint32_t(previous);
    return status;
}

// ---- Mutants -------------------------------------------------------------------

uint32_t NtCreateMutant(be<uint32_t>* handle, XOBJECT_ATTRIBUTES* attributes, uint32_t initialOwner)
{
    CheckUnnamed(attributes, "NtCreateMutant");
    *handle = GetKernelHandle(CreateKernelObject<MutantObject>(initialOwner != 0));
    return STATUS_SUCCESS;
}

uint32_t NtReleaseMutant(uint32_t handle, be<uint32_t>* previousCount)
{
    auto* m = FromHandle<MutantObject>(handle);
    if (m == nullptr)
        return STATUS_INVALID_HANDLE;
    int32_t previous = 0;
    uint32_t status = m->Release(&previous);
    // Xenia notes the second argument is unused by titles it has seen.
    return status;
}

// ---- Timers --------------------------------------------------------------------

uint32_t NtCreateTimer(be<uint32_t>* handle, XOBJECT_ATTRIBUTES* attributes, uint32_t timerType)
{
    CheckUnnamed(attributes, "NtCreateTimer");
    *handle = GetKernelHandle(CreateKernelObject<TimerObject>(timerType));
    return STATUS_SUCCESS;
}

uint32_t NtSetTimerEx(uint32_t handle, be<int64_t>* dueTime, uint32_t apcRoutine, uint32_t apcMode,
    uint32_t apcContext, uint32_t resume, uint32_t periodMs, be<uint32_t>* previousState)
{
    auto* t = FromHandle<TimerObject>(handle);
    if (t == nullptr)
        return STATUS_INVALID_HANDLE;
    if (apcRoutine != 0)
    {
        // Timer APCs need an APC queue on the target thread; make the first
        // use loud rather than never calling the routine.
        fprintf(stderr, "[sync] NtSetTimerEx with an APC routine (%08X) is not supported yet\n", apcRoutine);
        assert(false && "timer APC routines are not implemented");
    }
    bool previous = t->Set(Deadline(dueTime), periodMs);
    if (previousState)
        *previousState = previous;
    return STATUS_SUCCESS;
}

uint32_t NtCancelTimer(uint32_t handle, be<uint32_t>* currentState)
{
    auto* t = FromHandle<TimerObject>(handle);
    if (t == nullptr)
        return STATUS_INVALID_HANDLE;
    bool state = t->Cancel();
    if (currentState)
        *currentState = state;
    return STATUS_SUCCESS;
}

// ---- Waits ---------------------------------------------------------------------

uint32_t KeWaitForSingleObject(XDISPATCHER_HEADER* object, uint32_t waitReason, uint32_t waitMode,
    uint32_t alertable, be<int64_t>* timeout)
{
    Waitable* w = BindDispatcherHeader(*object);
    if (w == nullptr)
        return STATUS_INVALID_PARAMETER;
    return WaitForObjects({ &w, 1 }, false, Deadline(timeout));
}

uint32_t KeWaitForMultipleObjects(uint32_t count, xpointer<XDISPATCHER_HEADER>* objects, uint32_t waitType,
    uint32_t waitReason, uint32_t waitMode, uint32_t alertable, be<int64_t>* timeout, void* waitBlocks)
{
    std::vector<Waitable*> list;
    for (uint32_t i = 0; i < count; i++)
    {
        Waitable* w = BindDispatcherHeader(*objects[i]);
        if (w == nullptr)
            return STATUS_INVALID_PARAMETER;
        list.push_back(w);
    }
    // waitType: 0 = WaitAll, 1 = WaitAny.
    return WaitForObjects(list, waitType == 0, Deadline(timeout));
}

uint32_t NtWaitForSingleObjectEx(uint32_t handle, uint32_t waitMode, uint32_t alertable, be<int64_t>* timeout)
{
    be<uint32_t> h = handle;
    return WaitHandles({ &h, 1 }, false, timeout);
}

uint32_t NtWaitForMultipleObjectsEx(uint32_t count, be<uint32_t>* handles, uint32_t waitType, uint32_t waitMode,
    uint32_t alertable, be<int64_t>* timeout)
{
    return WaitHandles({ handles, count }, waitType == 0, timeout);
}

uint32_t NtSignalAndWaitForSingleObjectEx(uint32_t signalHandle, uint32_t waitHandle, uint32_t alertable,
    uint32_t unknown, be<int64_t>* timeout)
{
    KernelObject* signal = IsKernelObject(signalHandle) ? GetKernelObject(signalHandle) : nullptr;
    if (auto* e = dynamic_cast<EventObject*>(signal))
        e->Set();
    else if (auto* s = dynamic_cast<SemaphoreObject*>(signal))
        s->Release(1, nullptr);
    else if (auto* m = dynamic_cast<MutantObject*>(signal))
        m->Release(nullptr);
    else
        return STATUS_OBJECT_TYPE_MISMATCH;
    be<uint32_t> h = waitHandle;
    return WaitHandles({ &h, 1 }, false, timeout);
}

// ---- Delays --------------------------------------------------------------------

#if defined(__APPLE__)
namespace
{
    std::atomic<uint64_t> s_sleeps{ 0 }, s_sleptNs{ 0 }, s_maxNs{ 0 }, s_late{ 0 };
    std::atomic<int64_t> s_nextLog{ 0 }, s_windowFrom{ 0 };
    void LogBackOffSummary(int64_t now, const char* label);

    int64_t SteadyNs()
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    // RECARBON_SLEEP0_BACKOFF's evidence: each thread's first sleep (its QoS
    // class decides how far macOS may stretch its timers), and the first late
    // ones (each makes that thread yield again for a second).
    void NoteBackOffSleep(bool late, int64_t ns)
    {
        thread_local bool noted = false;
        if (!noted)
        {
            noted = true;
            char name[64] = {};
            pthread_getname_np(pthread_self(), name, sizeof(name));
            qos_class_t qos = QOS_CLASS_UNSPECIFIED;
            int relative = 0;
            pthread_get_qos_class_np(pthread_self(), &qos, &relative);
            fprintf(stderr, "[kernel] Sleep(0) back-off: thread %s sleeps (QoS class 0x%x), first sleep %.0f us\n", name[0] ? name : "?",
                unsigned(qos), ns / 1000.0);
        }
        static std::atomic<int> s_lateLogged{ 0 };
        if (late && s_lateLogged.fetch_add(1, std::memory_order_relaxed) < 20)
        {
            char name[64] = {};
            pthread_getname_np(pthread_self(), name, sizeof(name));
            fprintf(stderr, "[kernel] Sleep(0) back-off: a back-off sleep took %.1f ms on %s: it yields for 1 s\n", ns / 1e6, name[0] ? name : "?");
        }
        // Every 10 s (all threads), and at a RECARBON_SLEEP0_BACKOFF_AT
        // switch: how many sleeps, how long they took.
        s_sleeps.fetch_add(1, std::memory_order_relaxed);
        s_sleptNs.fetch_add(uint64_t(ns), std::memory_order_relaxed);
        for (uint64_t max = s_maxNs.load(std::memory_order_relaxed); uint64_t(ns) > max;)
            if (s_maxNs.compare_exchange_weak(max, uint64_t(ns), std::memory_order_relaxed))
                break;
        if (late)
            s_late.fetch_add(1, std::memory_order_relaxed);
        const int64_t now = SteadyNs();
        int64_t next = s_nextLog.load(std::memory_order_relaxed);
        if (next == 0)
        {
            s_nextLog.compare_exchange_strong(next, now + 10'000'000'000);
            s_windowFrom.store(now, std::memory_order_relaxed);
        }
        else if (now >= next && s_nextLog.compare_exchange_strong(next, now + 10'000'000'000))
            LogBackOffSummary(now, "");
    }

    // The sleeps since the last line (any thread may add one meanwhile: a
    // sleep at the edge can land in either line).
    void LogBackOffSummary(int64_t now, const char* label)
    {
        const uint64_t sleeps = s_sleeps.exchange(0), slept = s_sleptNs.exchange(0), max = s_maxNs.exchange(0), lates = s_late.exchange(0);
        const int64_t from = s_windowFrom.exchange(now);
        const double secs = from ? double(now - from) / 1e9 : 0.0;
        fprintf(stderr, "[kernel] Sleep(0) back-off%s: %llu sleeps (%.0f/s), avg %.1f us, max %.1f us, %llu late\n", label,
            (unsigned long long)sleeps, secs > 0 ? sleeps / secs : 0.0, sleeps ? slept / 1e3 / sleeps : 0.0, max / 1e3, (unsigned long long)lates);
    }

    // RECARBON_SLEEP0_BACKOFF_AT=<s>,... (a diagnostic, off by default):
    // kernel/sleep0_schedule.h. Seconds from here (static initialisation,
    // about the launch).
    const auto s_launch = std::chrono::steady_clock::now();
    const std::vector<double> s_backOffAt = [] {
        const char* v = std::getenv("RECARBON_SLEEP0_BACKOFF_AT");
        std::vector<double> times = kernel::sleep0schedule::ParseTimes(v);
        if (v && times.empty())
            fprintf(stderr, "[kernel] RECARBON_SLEEP0_BACKOFF_AT=%s ignored: ascending seconds since launch, such as 240,270.5\n", v);
        else if (v)
            fprintf(stderr, "[kernel] RECARBON_SLEEP0_BACKOFF_AT: %zu switches of the Sleep(0) back-off, the first at %.1f s\n", times.size(), times[0]);
        return times;
    }();
    std::atomic<int> s_switchLogged{ 0 };

    // macOS: RECARBON_SLEEP0_BACKOFF=1 backs off to 100 us sleeps by
    // kernel/sleep0_backoff.h's policy; =2 adds kernel/sleep0_predict.h's
    // predicted wake (an experiment). Off by default here (0).
    // Off by default (an experiment): in v0.1.3's release comparison at 2x the back-off cost race
    // pacing with the GPU-wait back-off on (frame threads included: a 30 fps lock; excluded: p99
    // 43-51 ms against 19-27), where the GPU-wait back-off alone matched or beat v0.1.2. The
    // front end's saving (-40% CPU) stays an opt-in: RECARBON_SLEEP0_BACKOFF=3 with the frame
    // threads excluded unless RECARBON_SLEEP0_BACKOFF_FRAME_THREADS=1.
    const int s_backOffMode = [] {
        const char* v = std::getenv("RECARBON_SLEEP0_BACKOFF");
        const int mode = !v ? 0 : v[0] == '1' ? 1 : v[0] == '2' ? 2 : v[0] == '3' ? 3 : 0;
        if (mode == 1)
            fprintf(stderr, "[kernel] RECARBON_SLEEP0_BACKOFF=1: Sleep(0) polls sleep 100 us after 1 ms of polling\n");
        else if (mode == 2)
            fprintf(stderr, "[kernel] RECARBON_SLEEP0_BACKOFF=2: Sleep(0) polls sleep 100 us after 1 ms, waking 1.5 ms before a predicted end\n");
        else if (mode == 3)
            fprintf(stderr, "[kernel] RECARBON_SLEEP0_BACKOFF=3: as =2, with 1 ms sleeps while more than 2 ms remain before a predicted end\n");
        return mode;
    }();

    // RECARBON_SLEEP0_BACKOFF_FRAME_THREADS=1: frame threads back off too; by default they keep yielding (sleep0_backoff.h).
    const bool s_excludeFrameThreads = [] {
        const char* v = std::getenv("RECARBON_SLEEP0_BACKOFF_FRAME_THREADS");
        const bool exclude = !(v && v[0] == '1');  // the default: frame threads keep yielding
        if (v && v[0] == '1')
            fprintf(stderr, "[kernel] RECARBON_SLEEP0_BACKOFF_FRAME_THREADS=1: frame threads back off too (experimental)\n");
        return exclude;
    }();

    const char* BackOffName(bool on)
    {
        return !on ? "off" : s_backOffMode == 3 ? "predicted-long" : s_backOffMode == 2 ? "predicted" : "on";
    }
}
#endif

#if defined(__APPLE__) && !TARGET_OS_IOS
namespace
{
    namespace trace = kernel::sleep0trace;

    // RECARBON_SLEEP0_TRACE=1 (a diagnostic, off by default): kernel/sleep0_trace.h.
    const bool s_sleep0Trace = [] {
        const char* v = std::getenv("RECARBON_SLEEP0_TRACE");
        const bool on = v && v[0] == '1';
        if (on)
            fprintf(stderr, "[sleep0] RECARBON_SLEEP0_TRACE=1: a line per polling thread every 10 s\n");
        return on;
    }();

    int SysctlInt(const char* name)
    {
        int value = 0;
        size_t size = sizeof(value);
        return sysctlbyname(name, &value, &size, nullptr, 0) == 0 ? value : -1;
    }

    // The device tree's cpu nodes (logical-cpu-id, cluster-type), checked
    // against hw.ncpu and hw.perflevel1 (efficiency) / hw.perflevel0
    // (performance) counts.
    trace::CpuLayout ReadCpuLayout()
    {
        std::vector<trace::CpuNode> nodes;
        if (io_registry_entry_t cpus = IORegistryEntryFromPath(MACH_PORT_NULL, "IODeviceTree:/cpus"))
        {
            io_iterator_t it = 0;
            if (IORegistryEntryGetChildIterator(cpus, kIODeviceTreePlane, &it) == KERN_SUCCESS)
            {
                while (io_registry_entry_t cpu = IOIteratorNext(it))
                {
                    int id = -1;
                    char type = 0;
                    if (CFTypeRef v = IORegistryEntryCreateCFProperty(cpu, CFSTR("logical-cpu-id"), kCFAllocatorDefault, 0))
                    {
                        if (CFGetTypeID(v) == CFNumberGetTypeID())
                            CFNumberGetValue(CFNumberRef(v), kCFNumberIntType, &id);
                        CFRelease(v);
                    }
                    if (CFTypeRef v = IORegistryEntryCreateCFProperty(cpu, CFSTR("cluster-type"), kCFAllocatorDefault, 0))
                    {
                        if (CFGetTypeID(v) == CFDataGetTypeID() && CFDataGetLength(CFDataRef(v)) > 0)
                            type = char(CFDataGetBytePtr(CFDataRef(v))[0]);
                        CFRelease(v);
                    }
                    nodes.push_back({ id, type });
                    IOObjectRelease(cpu);
                }
                IOObjectRelease(it);
            }
            IOObjectRelease(cpus);
        }
        const int ncpu = SysctlInt("hw.ncpu"), efficiency = SysctlInt("hw.perflevel1.logicalcpu"), performance = SysctlInt("hw.perflevel0.logicalcpu");
        trace::CpuLayout layout = trace::BuildLayout(nodes, ncpu, efficiency, performance);
        std::string list[2];
        for (int c = 0; c < ncpu && layout.verified; c++)
            list[layout.Of(c)] += std::format(" {}", c);
        if (layout.verified)
            fprintf(stderr, "[sleep0] cores verified: efficiency%s, performance%s\n", list[trace::kEfficiency].c_str(), list[trace::kPerformance].c_str());
        else
            fprintf(stderr, "[sleep0] cores not verified (device tree %zu cpus, hw.ncpu %d, perflevel1 %d, perflevel0 %d): clusters unknown\n",
                nodes.size(), ncpu, efficiency, performance);
        return layout;
    }

    trace::Cluster CurrentCluster()
    {
        static const trace::CpuLayout layout = ReadCpuLayout();
        size_t cpu = 0;
        return pthread_cpu_number_np(&cpu) == 0 ? layout.Of(int(cpu)) : trace::kUnknown;
    }

    // A zero delay's yield or sleep, from `start` to `end`; every 10 s, the thread's line.
    // The thread's line for its window: named with the phase when
    // RECARBON_SLEEP0_BACKOFF_AT is set (phase -1: not set).
    // `predicted`: with RECARBON_SLEEP0_BACKOFF=2, the thread's prediction
    // counts for the window, added to the line and started again.
    void TraceReport(trace::Tracker& t, int64_t now, int phase, bool backOff, kernel::sleep0predict::Stats* predicted)
    {
        char name[64] = {};
        pthread_getname_np(pthread_self(), name, sizeof(name));
        const std::string label = phase < 0 ? std::string(name[0] ? name : "?")
                                            : std::format("{} phase {} back-off {}", name[0] ? name : "?", phase, BackOffName(backOff));
        fprintf(stderr, "%s%s\n", t.Report(label.c_str(), now).c_str(), predicted ? predicted->Report().c_str() : "");
        if (predicted)
            *predicted = {};
        t.StartWindow(now);
    }

    void TraceDone(trace::Tracker& t, int64_t start, int64_t end, bool slept, trace::Cluster before, int phase, bool backOff,
        kernel::sleep0predict::Stats* predicted, int64_t askedNs = kernel::sleep0::kSleepNs)
    {
        t.Done(start, end, slept, before, CurrentCluster(), askedNs);
        if (t.Due(end))
            TraceReport(t, end, phase, backOff, predicted);
    }
}
#endif


uint32_t KeDelayExecutionThread(uint32_t waitMode, uint32_t alertable, be<int64_t>* interval)
{
    auto deadline = Deadline(interval);
    if (!deadline || *deadline <= guesttime::NowNs())
    {
        // While the game is suspended a polling loop would spin through it
        // (no guest time passes, so what it polls for can't come): park.
        guesttime::WaitWhileSuspended();

        // Sleep(0) polling loops: one game thread (g2) spent ~90% of a core
        // in them, 60% of it in sched_yield, and kept one of the Steam
        // Machine's two fast cores (see hostcpu::ReserveFastCore). After 1 ms
        // of back-to-back zero delays each one sleeps 100 us instead (a new
        // poll starts exact). Linux only: macOS coalesces default-QoS timers
        // to ~10 ms. RECARBON_SLEEP0_BACKOFF=0 always yields.
#ifdef __linux__
        static const bool backOff = [] { const char* v = std::getenv("RECARBON_SLEEP0_BACKOFF"); return !v || v[0] != '0'; }();
        thread_local Clock::time_point firstZero{}, lastZero{};
        auto now = Clock::now();
        if (now - lastZero > std::chrono::microseconds(300))
            firstZero = now;
        lastZero = now;
        if (backOff && now - firstZero > std::chrono::milliseconds(1))
        {
            hostcpu::TightenTimerSlack();
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            lastZero = Clock::now();
            return STATUS_SUCCESS;
        }
#endif
#if defined(__APPLE__)
        const bool macBackOff = s_backOffMode != 0;
        thread_local kernel::sleep0::State state;
        thread_local kernel::sleep0predict::State predicted;
#if !TARGET_OS_IOS
        thread_local kernel::sleep0trace::Tracker tracker;
#endif
        // RECARBON_SLEEP0_BACKOFF_AT: this moment's phase, from the clock alone.
        bool backOff = macBackOff;
        int phase = -1;
        if (!s_backOffAt.empty())
        {
            namespace schedule = kernel::sleep0schedule;
            thread_local schedule::ThreadPhase seen;
            const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - s_launch).count();
            const schedule::Step step = schedule::StepAt(s_backOffAt, macBackOff, seconds, seen, s_switchLogged);
            phase = step.phase;
            backOff = step.backOff;
            if (step.newPhase)
            {
#if !TARGET_OS_IOS
                if (s_sleep0Trace)
                {
                    tracker.CloseWait();
                    TraceReport(tracker, SteadyNs(), step.closedPhase, step.closedBackOff, s_backOffMode >= 2 ? &predicted.stats : nullptr);
                }
#endif
                state = {};  // a new phase's polls start their own runs
                predicted = {};
            }
            if (step.logSwitch)
            {
                LogBackOffSummary(SteadyNs(), std::format(" (to phase {})", phase).c_str());
                fprintf(stderr, "[kernel] RECARBON_SLEEP0_BACKOFF_AT: phase %d at %.1f s: back-off %s\n", phase, seconds, BackOffName(backOff));
            }
        }
#if !TARGET_OS_IOS
        int64_t traceAt = 0;
        kernel::sleep0trace::Cluster traceCluster = kernel::sleep0trace::kUnknown;
        if (s_sleep0Trace)
        {
            traceAt = SteadyNs();
            tracker.Call(traceAt);
            traceCluster = CurrentCluster();
        }
#endif
        backOff = kernel::sleep0::AppliesTo(backOff, kernel::IsFrameThread(), s_excludeFrameThreads);
        kernel::sleep0predict::Stats* const predictedStats = s_backOffMode >= 2 ? &predicted.stats : nullptr;
        if (backOff && s_backOffMode >= 2)
        {
            const bool longChunks = s_backOffMode == 3;
            const kernel::sleep0predict::Decision d = kernel::sleep0predict::Poll(predicted, SteadyNs(), longChunks);
            if (d.action == kernel::sleep0::Action::Sleep)
            {
                // (Slept and the trace's overrun count against 100 us: for a
                // shorter clamped sleep they under-report lateness.)
                const int64_t start = SteadyNs();
                timespec request{ 0, long(d.sleepNs) };
                nanosleep(&request, nullptr);
                const int64_t end = SteadyNs();
                const bool late = longChunks ? kernel::sleep0::SleptAsked(predicted.fixed, start, end, d.sleepNs)
                                             : kernel::sleep0::Slept(predicted.fixed, start, end);
                NoteBackOffSleep(late, end - start);
#if !TARGET_OS_IOS
                if (s_sleep0Trace)
                    TraceDone(tracker, start, end, true, traceCluster, phase, backOff, predictedStats, longChunks ? d.sleepNs : kernel::sleep0::kSleepNs);
#endif
                return STATUS_SUCCESS;
            }
        }
        else if (backOff)
        {
            if (kernel::sleep0::Poll(state, SteadyNs()) == kernel::sleep0::Action::Sleep)
            {
                const int64_t start = SteadyNs();
                timespec request{ 0, long(kernel::sleep0::kSleepNs) };
                nanosleep(&request, nullptr);
                const int64_t end = SteadyNs();
                NoteBackOffSleep(kernel::sleep0::Slept(state, start, end), end - start);
#if !TARGET_OS_IOS
                if (s_sleep0Trace)
                    TraceDone(tracker, start, end, true, traceCluster, phase, backOff, predictedStats);
#endif
                return STATUS_SUCCESS;
            }
        }
#endif
        std::this_thread::yield();
#if defined(__APPLE__) && !TARGET_OS_IOS
        if (s_sleep0Trace)
            TraceDone(tracker, traceAt, SteadyNs(), false, traceCluster, phase, backOff, predictedStats);
#endif
    }
    else
        guesttime::SleepUntil(*deadline);  // longer by any suspension
    return STATUS_SUCCESS;
}

uint32_t NtYieldExecution()
{
    guesttime::WaitWhileSuspended();  // as KeDelayExecutionThread's polls
    std::this_thread::yield();
    return STATUS_SUCCESS;
}

GUEST_FUNCTION_HOOK(__imp__NtCreateEvent, NtCreateEvent);
GUEST_FUNCTION_HOOK(__imp__KeSetEvent, KeSetEvent);
GUEST_FUNCTION_HOOK(__imp__KeResetEvent, KeResetEvent);
GUEST_FUNCTION_HOOK(__imp__KePulseEvent, KePulseEvent);
GUEST_FUNCTION_HOOK(__imp__NtSetEvent, NtSetEvent);
GUEST_FUNCTION_HOOK(__imp__NtClearEvent, NtClearEvent);
GUEST_FUNCTION_HOOK(__imp__NtPulseEvent, NtPulseEvent);
GUEST_FUNCTION_HOOK(__imp__KeInitializeSemaphore, KeInitializeSemaphore);
GUEST_FUNCTION_HOOK(__imp__KeReleaseSemaphore, KeReleaseSemaphore);
GUEST_FUNCTION_HOOK(__imp__NtCreateSemaphore, NtCreateSemaphore);
GUEST_FUNCTION_HOOK(__imp__NtReleaseSemaphore, NtReleaseSemaphore);
GUEST_FUNCTION_HOOK(__imp__NtCreateMutant, NtCreateMutant);
GUEST_FUNCTION_HOOK(__imp__NtReleaseMutant, NtReleaseMutant);
GUEST_FUNCTION_HOOK(__imp__NtCreateTimer, NtCreateTimer);
GUEST_FUNCTION_HOOK(__imp__NtSetTimerEx, NtSetTimerEx);
GUEST_FUNCTION_HOOK(__imp__NtCancelTimer, NtCancelTimer);
GUEST_FUNCTION_HOOK(__imp__KeWaitForSingleObject, KeWaitForSingleObject);
GUEST_FUNCTION_HOOK(__imp__KeWaitForMultipleObjects, KeWaitForMultipleObjects);
GUEST_FUNCTION_HOOK(__imp__NtWaitForSingleObjectEx, NtWaitForSingleObjectEx);
GUEST_FUNCTION_HOOK(__imp__NtWaitForMultipleObjectsEx, NtWaitForMultipleObjectsEx);
GUEST_FUNCTION_HOOK(__imp__NtSignalAndWaitForSingleObjectEx, NtSignalAndWaitForSingleObjectEx);
GUEST_FUNCTION_HOOK(__imp__KeDelayExecutionThread, KeDelayExecutionThread);
GUEST_FUNCTION_HOOK(__imp__NtYieldExecution, NtYieldExecution);

// Critical regions disable normal kernel APC delivery. We don't deliver
// APCs yet (their first use asserts), so there is nothing to disable.
void KeEnterCriticalRegion() {}
void KeLeaveCriticalRegion() {}

GUEST_FUNCTION_HOOK(__imp__KeEnterCriticalRegion, KeEnterCriticalRegion);
GUEST_FUNCTION_HOOK(__imp__KeLeaveCriticalRegion, KeLeaveCriticalRegion);

// Ends the calling thread: unwinds to GuestThread::Start, which then marks the
// thread finished (waking waiters) and exits it.
void ExTerminateThread(uint32_t exitStatus)
{
    throw GuestThreadExit{ exitStatus };
}

GUEST_FUNCTION_HOOK(__imp__ExTerminateThread, ExTerminateThread);

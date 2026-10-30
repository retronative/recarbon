// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// The frame threads' QoS (kernel/thread_priority.h, thread_priority.cpp): a
// guest thread that presents (VdSwap) or polls the controller
// (XamInputGetState) is raised to USER_INITIATED once, never lowered. Under CPU
// load on an M1 the game's frame threads at the default QoS left 100 ms+ frame
// gaps (ROADMAP M7). Real pthreads; the two handlers' sources are read.
// From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/frame_thread_qos_test.cpp runtime/kernel/thread_priority.cpp -o build/frame_thread_qos_test
//   build/frame_thread_qos_test runtime/gpu/imports_video.cpp runtime/hid/input.cpp
#include <kernel/thread_priority.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#if defined(__APPLE__)
#include <pthread.h>
#endif

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

#if defined(__APPLE__)
    qos_class_t Current()
    {
        qos_class_t qos = QOS_CLASS_UNSPECIFIED;
        pthread_get_qos_class_np(pthread_self(), &qos, nullptr);
        return qos;
    }

    // `body` on a new pthread that starts at `start`.
    template <typename F>
    void OnThread(qos_class_t start, F body)
    {
        std::thread([&] {
            pthread_set_qos_class_self_np(start, 0);
            body();
        }).join();
    }
#endif

    // The first statement of `signature`'s body in `path` (comments and
    // blank lines skipped).
    std::string FirstStatement(const char* path, const std::string& signature)
    {
        std::ifstream in(path);
        std::stringstream ss;
        ss << in.rdbuf();
        const std::string src = ss.str();
        size_t at = src.find("\n" + signature);
        if (at == std::string::npos)
            return {};
        at = src.find("\n{\n", at);
        if (at == std::string::npos)
            return {};
        std::istringstream body(src.substr(at + 3));
        for (std::string line; std::getline(body, line);)
        {
            size_t first = line.find_first_not_of(" \t");
            if (first == std::string::npos || line.compare(first, 2, "//") == 0)
                continue;
            line = line.substr(first);
            return line.substr(0, line.find(';') + 1);
        }
        return {};
    }
}

int main(int argc, char** argv)
{
#if defined(__APPLE__)
    using kernel::RaisesFrameThreadQos;

    // 1. The decision: raised from below USER_INITIATED, never lowered.
    static_assert(RaisesFrameThreadQos(QOS_CLASS_DEFAULT));
    Check(RaisesFrameThreadQos(QOS_CLASS_DEFAULT), "default is raised");
    Check(RaisesFrameThreadQos(QOS_CLASS_UTILITY), "utility is raised");
    Check(RaisesFrameThreadQos(QOS_CLASS_BACKGROUND), "background is raised");
    Check(RaisesFrameThreadQos(QOS_CLASS_UNSPECIFIED), "unspecified is raised");
    Check(!RaisesFrameThreadQos(QOS_CLASS_USER_INITIATED), "user-initiated is kept");
    Check(!RaisesFrameThreadQos(QOS_CLASS_USER_INTERACTIVE), "user-interactive is kept (never lowered)");

    // 2. A thread at the default is raised.
    OnThread(QOS_CLASS_DEFAULT, [] {
        Check(Current() == QOS_CLASS_DEFAULT, "the thread starts at the default");
        kernel::RaiseFrameThreadQos();
        Check(Current() == QOS_CLASS_USER_INITIATED, "a default thread is raised to user-initiated");
    });

    // 3. A user-interactive thread (as the runtime's presenter, CP and audio
    // threads are, or XAudio's mixer from priority 15) stays user-interactive.
    OnThread(QOS_CLASS_USER_INTERACTIVE, [] {
        kernel::RaiseFrameThreadQos();
        Check(Current() == QOS_CLASS_USER_INTERACTIVE, "a user-interactive thread is never lowered");
    });

    // 4. Once per thread: a later change isn't undone by the next call.
    OnThread(QOS_CLASS_UTILITY, [] {
        kernel::RaiseFrameThreadQos();
        Check(Current() == QOS_CLASS_USER_INITIATED, "a utility thread is raised to user-initiated");
        pthread_set_qos_class_self_np(QOS_CLASS_DEFAULT, 0);
        kernel::RaiseFrameThreadQos();
        kernel::RaiseFrameThreadQos();
        Check(Current() == QOS_CLASS_DEFAULT, "it applies once per thread: a later change is kept");
    });

    // 5. Another thread is unaffected, and gets its own first call.
    OnThread(QOS_CLASS_DEFAULT, [] {
        OnThread(QOS_CLASS_DEFAULT, [] { kernel::RaiseFrameThreadQos(); });
        Check(Current() == QOS_CLASS_DEFAULT, "another thread's raise leaves this one at the default");
        kernel::RaiseFrameThreadQos();
        Check(Current() == QOS_CLASS_USER_INITIATED, "this thread's own first call raises it");
    });
#endif

    // 5b. The thread is a frame thread from its first call on; others aren't.
    OnThread(QOS_CLASS_DEFAULT, [] {
        Check(!kernel::IsFrameThread(), "a fresh thread isn't a frame thread");
        kernel::RaiseFrameThreadQos();
        Check(kernel::IsFrameThread(), "after its first call it is");
        OnThread(QOS_CLASS_DEFAULT, [] { Check(!kernel::IsFrameThread(), "another thread still isn't"); });
        Check(kernel::IsFrameThread(), "and it stays one");
    });

    // 5c. RECARBON_FRAME_THREAD_QOS=interactive (an experiment): the target class.
    using kernel::FrameThreadQos;
    using kernel::ParseFrameThreadQos;
    Check(ParseFrameThreadQos(nullptr) == FrameThreadQos::Initiated && ParseFrameThreadQos("") == FrameThreadQos::Initiated, "unset or empty: user-initiated");
    Check(ParseFrameThreadQos("interactive") == FrameThreadQos::Interactive, "interactive");
    Check(ParseFrameThreadQos("interactive2") == FrameThreadQos::Initiated && ParseFrameThreadQos("Interactive") == FrameThreadQos::Initiated &&
              ParseFrameThreadQos("initiated") == FrameThreadQos::Initiated && ParseFrameThreadQos("1") == FrameThreadQos::Initiated,
        "anything else: user-initiated");
    Check(RaisesFrameThreadQos(QOS_CLASS_DEFAULT, FrameThreadQos::Interactive) && RaisesFrameThreadQos(QOS_CLASS_USER_INITIATED, FrameThreadQos::Interactive),
        "to interactive: default and user-initiated are raised");
    Check(!RaisesFrameThreadQos(QOS_CLASS_USER_INTERACTIVE, FrameThreadQos::Interactive), "user-interactive is kept");
    Check(!RaisesFrameThreadQos(QOS_CLASS_USER_INITIATED, FrameThreadQos::Initiated), "to initiated: user-initiated is kept (as before)");
    OnThread(QOS_CLASS_DEFAULT, [] {
        kernel::RaiseFrameThreadQosTo(FrameThreadQos::Interactive);
        Check(Current() == QOS_CLASS_USER_INTERACTIVE && kernel::IsFrameThread(), "raised to user-interactive on request");
    });
    OnThread(QOS_CLASS_USER_INITIATED, [] {
        kernel::RaiseFrameThreadQosTo(FrameThreadQos::Interactive);
        Check(Current() == QOS_CLASS_USER_INTERACTIVE, "a user-initiated thread is raised to user-interactive on request");
    });
    OnThread(QOS_CLASS_USER_INTERACTIVE, [] {
        kernel::RaiseFrameThreadQosTo(FrameThreadQos::Initiated);
        Check(Current() == QOS_CLASS_USER_INTERACTIVE, "never lowered");
    });
    OnThread(QOS_CLASS_DEFAULT, [] {
        kernel::RaiseFrameThreadQosTo(FrameThreadQos::Initiated);
        pthread_set_qos_class_self_np(QOS_CLASS_DEFAULT, 0);
        kernel::RaiseFrameThreadQosTo(FrameThreadQos::Interactive);
        Check(Current() == QOS_CLASS_DEFAULT, "the explicit form applies once per thread too");
    });

    // 5d. RECARBON_FRAME_THREAD_QOS_AT: the class per phase.
    using kernel::FrameThreadQosInPhase;
    Check(FrameThreadQosInPhase(FrameThreadQos::Initiated, 0) == FrameThreadQos::Initiated && FrameThreadQosInPhase(FrameThreadQos::Initiated, 1) == FrameThreadQos::Interactive &&
              FrameThreadQosInPhase(FrameThreadQos::Initiated, 2) == FrameThreadQos::Initiated && FrameThreadQosInPhase(FrameThreadQos::Initiated, 3) == FrameThreadQos::Interactive,
        "from user-initiated: interactive in odd phases");
    Check(FrameThreadQosInPhase(FrameThreadQos::Interactive, 0) == FrameThreadQos::Interactive && FrameThreadQosInPhase(FrameThreadQos::Interactive, 1) == FrameThreadQos::Initiated,
        "from user-interactive: the other way round");

    using kernel::SwitchedQosClass;
    Check(SwitchedQosClass(QOS_CLASS_DEFAULT, FrameThreadQos::Initiated) == QOS_CLASS_USER_INITIATED && SwitchedQosClass(QOS_CLASS_DEFAULT, FrameThreadQos::Interactive) == QOS_CLASS_USER_INTERACTIVE,
        "a switch sets the phase's class");
    Check(SwitchedQosClass(QOS_CLASS_USER_INTERACTIVE, FrameThreadQos::Initiated) == QOS_CLASS_USER_INTERACTIVE, "but never below a thread's own user-interactive");

    // 6. Both handlers call it first. Read from their sources: VdSwap and
    // XamInputGetState can't run in a unit test (they need the command
    // processor's ring and guest memory, and SDL's input state).
    if (argc < 3)
    {
        printf("usage: frame_thread_qos_test <gpu/imports_video.cpp> <hid/input.cpp>\n");
        return 2;
    }
    const std::string call = "kernel::RaiseFrameThreadQos();";
    Check(FirstStatement(argv[1], "void VdSwap(") == call, "VdSwap calls RaiseFrameThreadQos first");
    Check(FirstStatement(argv[2], "uint32_t XamInputGetState(") == call, "XamInputGetState calls RaiseFrameThreadQos first");

    if (g_failures)
    {
        printf("frame_thread_qos_test: %d failed\n", g_failures);
        return 1;
    }
    printf("frame_thread_qos_test: all passed\n");
    return 0;
}

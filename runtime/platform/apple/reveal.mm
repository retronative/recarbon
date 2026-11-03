// ReCarbon runtime. GPL-3.0-or-later (see COPYING). See ../reveal.h.
//
// macOS only (AppKit); on iOS, which builds every platform/apple/*.mm, there
// is no Finder. Manual reference counting like the rest of the runtime.
#include <TargetConditionals.h>
#include <platform/reveal.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#endif

namespace platform
{
    bool CanReveal()
    {
        return TARGET_OS_OSX != 0;
    }

    bool RevealInFinder(const std::filesystem::path& path)
    {
#if TARGET_OS_OSX
        std::error_code ec;
        if (path.empty() || !std::filesystem::is_regular_file(path, ec))
            return false;
        @autoreleasepool
        {
            NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path.c_str()]];
            [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[ url ]];
            return true;
        }
#else
        (void)path;
        return false;
#endif
    }
}

// Stream mode from the host display (app/backend/hostdisplayfit). The reference values are the
// Android client's HostDisplayFitTest / HostDisplayWatchTest ones: both clients pick the same mode.

#include "../../app/backend/hostdisplayfit.h"

#include <cstdio>

namespace {
int failures = 0;

void check(bool condition, const char* description)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        ++failures;
    }
}

bool is(const HostDisplayFit::Mode& m, int w, int h, int fps)
{
    return m.width == w && m.height == h && m.fps == fps;
}

void fitTests()
{
    check(is(HostDisplayFit::fit(1920, 1080, 60, 2400, 1080, 90), 1920, 1080, 60), "hostSmallerThanSetting");
    check(is(HostDisplayFit::fit(2400, 1080, 90, 1920, 1080, 60), 1920, 864, 60), "phoneSessionOnLaptop");
    check(is(HostDisplayFit::fit(2400, 1080, 90, 1920, 1080, 120), 1920, 864, 90), "fpsCappedByHost");
    check(is(HostDisplayFit::fit(1280, 720, 60, 1920, 1080, 60), 1280, 720, 60), "neverUpscale");
    check(is(HostDisplayFit::fit(3840, 2160, 60, 1920, 1080, 60), 1920, 1080, 60), "fourKIntoFullHd");
    check(is(HostDisplayFit::fit(1366, 768, 60, 1280, 720, 60), 1280, 718, 60), "roundsDownToEven");
    check(is(HostDisplayFit::fit(1920, 1080, 1000, 1920, 1080, 90), 1920, 1080, 90), "vrrDisplayKeepsClientFps");
    check(HostDisplayFit::applies(true, true), "appliesWhenResumingEvenWithVirtualDisplay");
    check(HostDisplayFit::applies(false, false), "appliesWhenLaunchingWithoutVirtualDisplay");
    check(!HostDisplayFit::applies(false, true), "notWhenLaunchingWithVirtualDisplay");
}

void watchTests()
{
    {
        // Streaming a 2400x1080 host at 1920x864 is the fitted state, not a change to chase
        HostDisplayWatch w(1920, 1080, 60, 1920, 864, 60);
        bool any = false;
        for (int i = 0; i < 3; i++) {
            any |= w.onPoll(true, 2400, 1080, 90).has_value();
        }
        check(!any, "scaledButStableDoesNotTrigger");
    }
    {
        HostDisplayWatch w(1920, 1080, 60, 1920, 864, 60);
        bool first = w.onPoll(true, 1920, 1080, 60).has_value();
        auto second = w.onPoll(true, 1920, 1080, 60);
        check(!first && second && is(*second, 1920, 1080, 60), "triggersAfterTwoConsecutive");
    }
    {
        HostDisplayWatch w(1920, 1080, 60, 1920, 1080, 60);
        bool a = w.onPoll(true, 1280, 720, 60).has_value();
        bool b = w.onPoll(true, 1600, 900, 60).has_value();
        auto c = w.onPoll(true, 1600, 900, 60);
        check(!a && !b && c && is(*c, 1600, 900, 60), "changingTargetRestartsDebounce");
    }
    {
        HostDisplayWatch w(1920, 1080, 60, 1920, 1080, 60);
        bool a = w.onPoll(true, 1280, 720, 60).has_value();
        bool b = w.onPoll(false, 0, 0, 0).has_value();
        bool c = w.onPoll(true, 1280, 720, 60).has_value();
        auto d = w.onPoll(true, 1280, 720, 60);
        check(!a && !b && !c && d && is(*d, 1280, 720, 60), "absentModeResets");
    }
    {
        HostDisplayWatch w(1920, 1080, 60, 1920, 1080, 60);
        w.onPoll(true, 1280, 720, 60);
        w.onPoll(true, 1280, 720, 60);
        bool third = w.onPoll(true, 1280, 720, 60).has_value();
        bool fourth = w.onPoll(true, 1280, 720, 60).has_value();
        check(!third && !fourth, "firesOnce");
    }
}
}

int main()
{
    fitTests();
    watchTests();
    if (failures == 0) {
        std::printf("tst_hostdisplay: all passed\n");
    }
    return failures == 0 ? 0 : 1;
}

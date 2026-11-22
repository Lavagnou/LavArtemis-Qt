#pragma once

#include <optional>

// Stream at the host display's mode when this client did not create that display.
//
// Joining a running session (or launching without a virtual display) streams whatever the host
// shows: asking for this client's own size made the host pad the image with black bars, and
// asking for more frames than the display produces only paced empty frames. So: the host
// display's aspect, at the largest size that fits both the client's setting and the host display
// (never upscaled), at the lower of the two frame rates. Same rule and reference values as the
// Android client (utils/HostDisplayFit, utils/HostDisplayWatch).
namespace HostDisplayFit {
struct Mode {
    int width;
    int height;
    int fps;

    bool operator==(const Mode& o) const { return width == o.width && height == o.height && fps == o.fps; }
    bool operator!=(const Mode& o) const { return !(*this == o); }
};

Mode fit(int hostW, int hostH, int hostRefresh, int settingW, int settingH, int settingFps);

// resuming: joining the session the host runs; launchWithVirtualDisplay: a new session whose
// display is created for this client (the only case the rule does not apply)
bool applies(bool resuming, bool launchWithVirtualDisplay);
}

// Decides when a host display mode change is worth reconnecting for: compares the fit *target*
// with the stream *in progress* (a 2400x1080 display streamed at 1920x864 is the fitted state,
// not a change), and wants the same new target on two consecutive polls (Windows passes through
// intermediate modes). Fires once.
class HostDisplayWatch {
public:
    HostDisplayWatch(int settingW, int settingH, int settingFps, int currentW, int currentH, int currentFps);

    // known = false when serverinfo did not carry the mode
    std::optional<HostDisplayFit::Mode> onPoll(bool known, int hostW, int hostH, int hostRefresh);

private:
    int m_SettingW, m_SettingH, m_SettingFps;
    HostDisplayFit::Mode m_Current;
    std::optional<HostDisplayFit::Mode> m_Pending;
    bool m_Fired = false;
};

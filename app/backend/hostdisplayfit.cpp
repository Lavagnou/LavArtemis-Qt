#include "hostdisplayfit.h"

#include <algorithm>
#include <cmath>

namespace HostDisplayFit {
namespace {
int even(int v)
{
    return std::max(2, v & ~1);
}
}

Mode fit(int hostW, int hostH, int hostRefresh, int settingW, int settingH, int settingFps)
{
    double scale = std::min(1.0, std::min(settingW / (double)hostW, settingH / (double)hostH));
    int w = even((int)std::floor(hostW * scale));
    int h = even((int)std::floor(hostH * scale));
    int fps = hostRefresh > 0 ? std::min(settingFps, hostRefresh) : settingFps;
    return Mode{w, h, fps};
}

bool applies(bool resuming, bool launchWithVirtualDisplay)
{
    return resuming || !launchWithVirtualDisplay;
}
}

HostDisplayWatch::HostDisplayWatch(int settingW, int settingH, int settingFps, int currentW, int currentH, int currentFps)
    : m_SettingW(settingW),
      m_SettingH(settingH),
      m_SettingFps(settingFps),
      m_Current{currentW, currentH, currentFps}
{
}

std::optional<HostDisplayFit::Mode> HostDisplayWatch::onPoll(bool known, int hostW, int hostH, int hostRefresh)
{
    if (m_Fired) {
        return std::nullopt;
    }
    if (!known) {
        m_Pending.reset();
        return std::nullopt;
    }
    HostDisplayFit::Mode target = HostDisplayFit::fit(hostW, hostH, hostRefresh, m_SettingW, m_SettingH, m_SettingFps);
    if (target == m_Current) {
        m_Pending.reset();
        return std::nullopt;
    }
    if (m_Pending && *m_Pending == target) {
        m_Fired = true;
        return target;
    }
    m_Pending = target;
    return std::nullopt;
}

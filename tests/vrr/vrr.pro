# VRR pacing tests, ported from Nonary/moonlight-qt with the VRR pacer
# (Linux presentation, Gamescope and haptics tests are not ported, nor is
# that code). Keep the platform-neutral timing test separate from the Qt
# policy test: the former deliberately has no Qt event-loop/runtime dependency.
TEMPLATE = subdirs
CONFIG += ordered

dxgipresent.file = $$PWD/dxgipresent.pro
presentationclock.file = $$PWD/presentationclock.pro
incomingtiming.file = $$PWD/incomingtiming.pro
vrrrenderpolicy.file = $$PWD/vrrrenderpolicy.pro
d3d11bindpolicy.file = $$PWD/d3d11bindpolicy.pro
timingcontroller.file = $$PWD/timingcontroller.pro
ratepolicy.file = $$PWD/ratepolicy.pro
diagnostics.file = $$PWD/diagnostics.pro
pacingworker.file = $$PWD/pacingworker.pro
replay.file = $$PWD/replay.pro
replayconfig.file = $$PWD/replayconfig.pro
queuesim.file = $$PWD/queuesim.pro
overlay.file = $$PWD/overlay.pro
profile.file = $$PWD/profile.pro

SUBDIRS += \
    dxgipresent \
    presentationclock \
    incomingtiming \
    vrrrenderpolicy \
    d3d11bindpolicy \
    timingcontroller \
    ratepolicy \
    diagnostics \
    pacingworker \
    replay \
    replayconfig \
    queuesim \
    overlay \
    profile

win32 {
    compositionprobe.file = $$PWD/compositionprobe.pro
    SUBDIRS += compositionprobe
}

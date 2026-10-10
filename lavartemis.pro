TEMPLATE = subdirs
SUBDIRS = \
    moonlight-common-c \
    qmdnsengine \
    app \
    h264bitstream

# Build the dependencies in parallel before the final app
app.depends = qmdnsengine moonlight-common-c h264bitstream
win32:!winrt {
    SUBDIRS += AntiHooking
    app.depends += AntiHooking
}

# PyroWave codec library (pyrowave/pyrowave.pro). Must match the condition that
# sets CONFIG += pyrowave in app/app.pro. x86_64 only for now: Granite's math
# headers have not been checked against MSVC ARM64.
win32:!winrt:contains(QT_ARCH, x86_64):!disable-pyrowave {
    SUBDIRS += pyrowave
    app.depends += pyrowave
}
linux:contains(QT_ARCH, x86_64):!disable-pyrowave:!disable-libplacebo:packagesExist(libplacebo) {
    SUBDIRS += pyrowave
    app.depends += pyrowave
}

# Support debug and release builds from command line for CI
CONFIG += debug_and_release

# Run our compile tests
load(configure)
qtCompileTest(SL)
qtCompileTest(EGL)

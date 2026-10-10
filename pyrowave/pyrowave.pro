# PyroWave codec: upstream C API plus the Granite subset it links, built from the
# Lavagnou/pyrowave submodule (branch lavartemis). The source list lives in the
# submodule (lavartemis/pyrowave.pri) so LavApollo, this client and the Android
# client compile the same thing. The bitstream has no version field: all three
# must pin the same commit (pyrowave/lavartemis/README.md).

QT -= core gui

TARGET = pyrowave
TEMPLATE = lib

# Build a static library
CONFIG += staticlib c++17

# Third-party code: keep its warnings out of our build logs
CONFIG += warn_off

# Include global qmake defs
include(../globaldefs.pri)

include(pyrowave/lavartemis/pyrowave.pri)

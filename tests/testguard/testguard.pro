# Test build logic: the allowlist (app/backend/testguard, no network) and the testbench
# command channel (app/testctl: protocol, and Qt Quick probing on an offscreen scene).
TEMPLATE = app
TARGET = tst_testguard
QT = core gui qml quick testlib
CONFIG += console c++17
CONFIG -= app_bundle
SOURCES += \
    $$PWD/tst_testguard.cpp \
    $$PWD/../../app/backend/testguard.cpp \
    $$PWD/../../app/testctl/testprotocol.cpp \
    $$PWD/../../app/testctl/uiprobe.cpp
HEADERS += \
    $$PWD/../../app/backend/testguard.h \
    $$PWD/../../app/testctl/testprotocol.h \
    $$PWD/../../app/testctl/uiprobe.h

# Test build allowlist (app/backend/testguard): pure logic, no network.
TEMPLATE = app
TARGET = tst_testguard
QT = core
CONFIG += console c++17
CONFIG -= app_bundle
SOURCES += \
    $$PWD/tst_testguard.cpp \
    $$PWD/../../app/backend/testguard.cpp
HEADERS += \
    $$PWD/../../app/backend/testguard.h

# Stream mode from the host display (app/backend/hostdisplayfit): pure logic, no streaming.
TEMPLATE = app
TARGET = tst_hostdisplay
QT = core
CONFIG += console c++17
CONFIG -= app_bundle
SOURCES += \
    $$PWD/tst_hostdisplay.cpp \
    $$PWD/../../app/backend/hostdisplayfit.cpp
HEADERS += \
    $$PWD/../../app/backend/hostdisplayfit.h

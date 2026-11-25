# The in-app update's rules (app/backend/updaterules): versions, Dev release, asset, cleanup.
TEMPLATE = app
TARGET = tst_updater
QT = core
CONFIG += console c++17
CONFIG -= app_bundle
SOURCES += \
    $$PWD/tst_updater.cpp \
    $$PWD/../../app/backend/updaterules.cpp
HEADERS += \
    $$PWD/../../app/backend/updaterules.h

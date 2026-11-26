# Settings profiles (app/settings/profilemanager): a new profile is a full snapshot of the settings in
# effect, with or without a profile active (Android's SettingsStore.snapshot), never a view of the globals.
TEMPLATE = app
TARGET = tst_profiles
QT = core qml
CONFIG += console c++17
CONFIG -= app_bundle
# testRawValue(): what a profile stores, without the fall-through
DEFINES += LAV_TEST_BUILD
INCLUDEPATH += $$PWD/../../app/settings
SOURCES += \
    $$PWD/tst_profiles.cpp \
    $$PWD/../../app/settings/profilemanager.cpp
HEADERS += \
    $$PWD/../../app/settings/profilemanager.h

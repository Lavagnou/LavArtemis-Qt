# Gamepad mappings (app/settings/mappingmanager): axis syntax, the guessed layout, and the guess as
# the app reports it across applyMappings() calls, against an SDL virtual joystick (no device needed).
TEMPLATE = app
TARGET = tst_mapping
QT = core network
CONFIG += console c++17
CONFIG -= app_bundle
# No MappingFetcher download (the test never touches the network)
DEFINES += LAV_TEST_BUILD
INCLUDEPATH += $$PWD/../../app $$PWD/../../app/settings
win32 {
    INCLUDEPATH += $$PWD/../../libs/windows/include/x64 $$PWD/../../libs/windows/include/x64/SDL2 $$PWD/../../libs/windows/include
    LIBS += -L$$PWD/../../libs/windows/lib/x64 -lSDL2
}
unix {
    CONFIG += link_pkgconfig
    PKGCONFIG += sdl2
}
SOURCES += \
    $$PWD/tst_mapping.cpp \
    $$PWD/../../app/settings/mappingmanager.cpp \
    $$PWD/../../app/path.cpp
HEADERS += \
    $$PWD/../../app/settings/mappingmanager.h

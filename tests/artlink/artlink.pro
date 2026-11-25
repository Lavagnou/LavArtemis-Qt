# art:// links and .art files (app/cli/artlink): parsing and the command line they become.
TEMPLATE = app
TARGET = tst_artlink
QT = core
CONFIG += console c++17
CONFIG -= app_bundle
SOURCES += \
    $$PWD/tst_artlink.cpp \
    $$PWD/../../app/cli/artlink.cpp
HEADERS += \
    $$PWD/../../app/cli/artlink.h

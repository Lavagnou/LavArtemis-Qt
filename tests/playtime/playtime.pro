# Play time text (app/backend/playtimetext): « 25 min jouées », « 3 h jouées », « Jamais lancé ».
TEMPLATE = app
TARGET = tst_playtime
QT = core
CONFIG += console c++17
CONFIG -= app_bundle
SOURCES += \
    $$PWD/tst_playtime.cpp \
    $$PWD/../../app/backend/playtimetext.cpp
HEADERS += \
    $$PWD/../../app/backend/playtimetext.h

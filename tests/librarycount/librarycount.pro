# The library's game count (app/backend/librarycounttext): « ATLAS-PC · 265 jeux », « 12 affichés sur 265 ».
TEMPLATE = app
TARGET = tst_librarycount
QT = core
CONFIG += console c++17
CONFIG -= app_bundle
SOURCES += \
    $$PWD/tst_librarycount.cpp \
    $$PWD/../../app/backend/librarycounttext.cpp
HEADERS += \
    $$PWD/../../app/backend/librarycounttext.h

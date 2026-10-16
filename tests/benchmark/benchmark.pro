# Benchmark planning, scoring and reports: pure logic, no streaming.
TEMPLATE = app
TARGET = tst_benchmark
QT = core
CONFIG += console c++17
CONFIG -= app_bundle
SOURCES += \
    $$PWD/tst_benchmark.cpp \
    $$PWD/../../app/backend/benchmark/benchmarktypes.cpp \
    $$PWD/../../app/backend/benchmark/benchmarkscoring.cpp \
    $$PWD/../../app/backend/benchmark/benchmarkplanner.cpp \
    $$PWD/../../app/backend/benchmark/benchmarkreport.cpp
HEADERS += \
    $$PWD/../../app/backend/benchmark/presenthistogram.h \
    $$PWD/../../app/backend/benchmark/benchmarktypes.h \
    $$PWD/../../app/backend/benchmark/benchmarkscoring.h \
    $$PWD/../../app/backend/benchmark/benchmarkplanner.h \
    $$PWD/../../app/backend/benchmark/benchmarkreport.h

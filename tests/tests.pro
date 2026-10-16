# The test tree is opt-in: the application and package builds never enter it.
# Build it with: qmake tests/tests.pro CONFIG+=tests
TEMPLATE = subdirs
CONFIG += ordered

contains(CONFIG, tests) {
    SUBDIRS += vrr benchmark
} else {
    message(Tests are disabled; rerun qmake with CONFIG+=tests)
}

// Gamepad mappings (app/settings/mappingmanager): how an axis is written, the layout guessed for a
// joystick SDL does not know, and that guess as the app reports it -- after every applyMappings()
// (the mapper's « Actualiser », each stream launch), not only the first; ignored at once when the
// user turns guessing off, back when it is on again; gone once a real mapping is saved.
// Uses an SDL virtual joystick: no device, no window.

// Our own main(), not SDL_main
#define SDL_MAIN_HANDLED
#include "mappingmanager.h"

#include "SDL_compat.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <cstdio>

namespace {
int failures = 0;
bool g_FallbackEnabled = true;

void check(bool condition, const char* description)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        ++failures;
    }
}

void testAxisSourceToken()
{
    check(MappingManager::axisSourceToken(2, -32768, 30000) == "a2", "resting at the low stop: the whole axis");
    check(MappingManager::axisSourceToken(2, 32767, -30000) == "a2~", "resting at the high stop: the whole axis, inverted");
    check(MappingManager::axisSourceToken(5, 0, 20000) == "+a5", "centred, pushed up: the upper half");
    check(MappingManager::axisSourceToken(5, 100, -20000) == "-a5", "centred, pushed down: the lower half");
}

void testSynthesize()
{
    const QString guid = "03000000aabb00001234000000000000";
    // Four axes (X, Y, Z, Rz): the right stick on Z/Rz, no analog trigger -- the RadioMaster TX15
    QString m = MappingManager::synthesizeMapping(guid, "Radio, TX15", 4, 8, 1, { 0, 0, 0, 0 }, "Windows");
    check(m.startsWith(guid + ",Radio  TX15,"), "GUID first, a comma in the name replaced");
    check(m.contains("leftx:a0,lefty:a1,rightx:a2,righty:a3"), "4 axes: right stick on Z/Rz");
    check(!m.contains("lefttrigger"), "4 axes: no trigger");
    check(m.contains("a:b0,b:b1,x:b2,y:b3,leftshoulder:b4,rightshoulder:b5,back:b6,start:b7"), "the 8 buttons in HID order");
    check(!m.contains("leftstick"), "no binding past the buttons the device has");
    check(m.contains("dpup:h0.1,dpright:h0.2,dpdown:h0.4,dpleft:h0.8"), "the hat as the D-pad");
    check(m.endsWith("platform:Windows"), "the platform last");

    // Six axes: triggers on Z/Rz, written by where they rest
    m = MappingManager::synthesizeMapping(guid, "Pad", 6, 11, 0, { 0, 0, -32768, 0, 0, 0 }, "Windows");
    check(m.contains("rightx:a3,righty:a4"), "6 axes: right stick on Rx/Ry");
    check(m.contains("lefttrigger:a2"), "a trigger resting at its stop: the whole axis");
    check(m.contains("righttrigger:+a5"), "a centred trigger: the upper half (never 50 % at rest)");
    check(m.contains("guide:b10") && !m.contains("dpup"), "11 buttons up to guide, no hat no D-pad");

    m = MappingManager::synthesizeMapping(guid, QString(), 4, 8, 0, {}, "Windows");
    check(m.startsWith(guid + ",Unknown Gamepad,"), "a device without a name");
    check(MappingManager::synthesizeMapping(guid, "Wheel", 3, 8, 0, {}, "Windows").isEmpty(), "3 axes: not a gamepad");
    check(MappingManager::synthesizeMapping(guid, "Box", 4, 6, 0, {}, "Windows").isEmpty(), "6 buttons: not a gamepad");
    check(MappingManager::synthesizeMapping(guid, "Hats", 4, 8, 2, {}, "Windows").isEmpty(), "2 hats: not a gamepad");
}

int findDevice(const char* name)
{
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        const char* n = SDL_JoystickNameForIndex(i);
        if (n != nullptr && strcmp(n, name) == 0) {
            return i;
        }
    }
    return -1;
}

void testGuessAcrossApplies()
{
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
        check(false, "SDL joystick subsystem");
        return;
    }
    const char* kName = "LavArtemis Unit Joystick";
    SDL_VirtualJoystickDesc desc;
    SDL_zero(desc);
    desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    desc.type = SDL_JOYSTICK_TYPE_UNKNOWN;
    desc.naxes = 4;
    desc.nbuttons = 8;
    desc.nhats = 1;
    desc.name = kName;
    if (SDL_JoystickAttachVirtualEx(&desc) < 0) {
        check(false, "SDL_JoystickAttachVirtualEx");
        return;
    }
    int index = findDevice(kName);
    check(index >= 0 && !SDL_IsGameController(index), "a generic joystick SDL has no mapping for");

    char guidStr[33];
    SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(index), guidStr, sizeof(guidStr));
    const QString guid = QString::fromLatin1(guidStr).toLower();

    MappingManager first;
    first.applyMappings();
    index = findDevice(kName);
    check(MappingManager::isGameController(index), "guessed: a game controller");
    check(MappingManager::getGuessedDeviceGuids().contains(guid), "the first apply reports the guess");
    check(MappingManager::getGuessedDeviceNames().contains(kName), "by name too");

    // SDL now knows the device (through our guess): a later apply must still call it a guess
    MappingManager second;
    second.applyMappings();
    check(MappingManager::getGuessedDeviceGuids().contains(guid), "the second apply still reports the guess");
    check(MappingManager::getGuessedDeviceNames().contains(kName), "and names it (stream launch warning)");

    // Guessing turned off: effective at once, though SDL cannot forget the mapping
    g_FallbackEnabled = false;
    MappingManager off;
    off.applyMappings();
    check(!MappingManager::isGameController(findDevice(kName)), "guessing off: no longer a game controller");
    check(MappingManager::isIgnoredGuess(findDevice(kName)), "an ignored guess");
    check(MappingManager::getGuessedDeviceGuids().isEmpty(), "guessing off: nothing reported as guessed");

    g_FallbackEnabled = true;
    MappingManager on;
    on.applyMappings();
    check(MappingManager::isGameController(findDevice(kName)), "on again: a game controller");
    check(MappingManager::getGuessedDeviceGuids().contains(guid), "on again: a guess again");

    // The user saves a real mapping while guessing is off (the mapper starts from the guess SDL
    // still has): the device is usable at once, not after a restart (review of waves 1-4, #1)
    g_FallbackEnabled = false;
    MappingManager offAgain;
    offAgain.applyMappings();
    check(MappingManager::isIgnoredGuess(findDevice(kName)), "off again: an ignored guess");
    MappingManager savedOff;
    savedOff.addMapping(guid + ",LavArtemis Unit Joystick,a:b0,b:b1,leftx:a0,lefty:a1,platform:" + SDL_GetPlatform());
    savedOff.applyMappings();
    check(MappingManager::isGameController(findDevice(kName)), "saved with guessing off: a game controller");
    check(!MappingManager::isIgnoredGuess(findDevice(kName)), "saved with guessing off: not an ignored guess");
    check(!MappingManager::getGuessedDeviceGuids().contains(guid), "saved with guessing off: not a guess");
    g_FallbackEnabled = true;

    // The user saves a real mapping (the mapper): no longer a guess
    MappingManager saved;
    saved.addMapping(guid + ",LavArtemis Unit Joystick,a:b1,b:b0,leftx:a0,lefty:a1,platform:" + SDL_GetPlatform());
    saved.applyMappings();
    check(!MappingManager::getGuessedDeviceGuids().contains(guid), "a saved mapping is not a guess");
    check(MappingManager::isGameController(findDevice(kName)), "and the device is a game controller");
    g_FallbackEnabled = false;
    check(MappingManager::isGameController(findDevice(kName)), "whatever guessing says");
    g_FallbackEnabled = true;

    SDL_JoystickDetachVirtual(findDevice(kName));
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
}
}

// The seam of mappingmanagersettings.cpp: the setting, as the test sets it
bool MappingManager::fallbackEnabled()
{
    return g_FallbackEnabled;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    // Nothing written to the user's settings: an INI file in a temporary folder, and test paths
    QTemporaryDir dir;
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
    QCoreApplication::setOrganizationName("LavArtemis Unit Tests");
    QCoreApplication::setApplicationName("tst_mapping");
    QDir::setCurrent(dir.path());

    testAxisSourceToken();
    testSynthesize();
    testGuessAcrossApplies();
    if (failures == 0) {
        std::printf("tst_mapping: all checks passed\n");
    }
    return failures == 0 ? 0 : 1;
}

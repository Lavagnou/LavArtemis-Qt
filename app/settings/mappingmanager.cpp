#include "mappingmanager.h"
#include "path.h"
#include "artemissettings.h"

#include <QDir>

#include "SDL_compat.h"

#define SER_GAMEPADMAPPING "gcmapping"

#define SER_GUID "guid"
#define SER_MAPPING "mapping"

// An axis resting at least this far out is resting against a stop rather than
// centered.
#define AXIS_EXTREME_REST 24000

MappingFetcher* MappingManager::s_MappingFetcher;
QStringList MappingManager::s_GuessedDeviceNames;
QStringList MappingManager::s_GuessedDeviceGuids;

MappingManager::MappingManager()
{
    QSettings settings;

    // Load updated mappings from the Internet once per Moonlight launch
    if (s_MappingFetcher == nullptr) {
        s_MappingFetcher = new MappingFetcher();
        s_MappingFetcher->start();
    }

    // First load existing saved mappings. This ensures the user's
    // hints can always override the old data.
    int mappingCount = settings.beginReadArray(SER_GAMEPADMAPPING);
    for (int i = 0; i < mappingCount; i++) {
        settings.setArrayIndex(i);

        SdlGamepadMapping mapping(settings.value(SER_GUID).toString(), settings.value(SER_MAPPING).toString());
        addMapping(mapping);
    }
    settings.endArray();

    // Finally load mappings from SDL_HINT_GAMECONTROLLERCONFIG
    QStringList sdlMappings =
            QString::fromLocal8Bit(SDL_GetHint(SDL_HINT_GAMECONTROLLERCONFIG))
        #if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            .split('\n', Qt::SkipEmptyParts);
        #else
            .split('\n', QString::SkipEmptyParts);
        #endif
    for (const QString& sdlMapping : std::as_const(sdlMappings)) {
        SdlGamepadMapping mapping(sdlMapping);
        addMapping(mapping);
    }

    // Save the updated mappings to settings
    save();
}

void MappingManager::save()
{
    QSettings settings;

    settings.remove(SER_GAMEPADMAPPING);
    settings.beginWriteArray(SER_GAMEPADMAPPING);
    QList<SdlGamepadMapping> mappings = m_Mappings.values();
    for (int i = 0; i < mappings.count(); i++) {
        settings.setArrayIndex(i);

        settings.setValue(SER_GUID, mappings[i].getGuid());
        settings.setValue(SER_MAPPING, mappings[i].getMapping());
    }
    settings.endArray();
}

void MappingManager::applyMappings()
{
    QByteArray mappingData = Path::readDataFile("gamecontrollerdb.txt");
    if (!mappingData.isEmpty()) {
        int newMappings = SDL_GameControllerAddMappingsFromRW(
                    SDL_RWFromConstMem(mappingData.constData(), mappingData.size()), 1);

        if (newMappings > 0) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Loaded %d new gamepad mappings",
                        newMappings);
        }
        else {
            if (newMappings < 0) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "Error loading gamepad mappings: %s",
                             SDL_GetError());
            }
            else if (newMappings == 0) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "0 new mappings found in gamecontrollerdb.txt. Is it corrupt?");
            }

            // Try deleting the cached mapping list just in case it's corrupt
            Path::deleteCacheFile("gamecontrollerdb.txt");
        }
    }
    else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unable to load gamepad mapping file");
    }

    QList<SdlGamepadMapping> mappings = m_Mappings.values();
    for (const SdlGamepadMapping& mapping : std::as_const(mappings)) {
        QString sdlMappingString = mapping.getSdlMappingString();
        int ret = SDL_GameControllerAddMapping(qPrintable(sdlMappingString));
        if (ret < 0) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Unable to add mapping: %s",
                        qPrintable(sdlMappingString));
        }
        else if (ret == 1) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Loaded saved user mapping: %s",
                        qPrintable(sdlMappingString));
        }
    }

    // Last, so that a real entry from the database or from the user always
    // wins over a guess. Doing it here rather than at each call site means
    // every path that applies mappings gets the fallback for free.
    if (ArtemisSettings::instance()->genericGamepadFallbackEnabled()) {
        applyFallbackMappings();
    }
}

QStringList MappingManager::getGuessedDeviceNames()
{
    return s_GuessedDeviceNames;
}

QStringList MappingManager::getGuessedDeviceGuids()
{
    return s_GuessedDeviceGuids;
}

QString MappingManager::axisSourceToken(int axis, int restValue, int deviation)
{
    if (restValue <= -AXIS_EXTREME_REST) {
        return QStringLiteral("a%1").arg(axis);
    }
    if (restValue >= AXIS_EXTREME_REST) {
        return QStringLiteral("a%1~").arg(axis);
    }
    return (deviation < 0) ? QStringLiteral("-a%1").arg(axis)
                           : QStringLiteral("+a%1").arg(axis);
}

QString MappingManager::synthesizeMapping(int deviceIndex, int numAxes, int numButtons, int numHats,
                                          const QVector<int>& axisRestValues)
{
    // Same shape test SdlInputHandler::getUnmappedGamepads() uses to decide a
    // bare joystick is probably a gamepad. Reusing it keeps the thing we warn
    // about and the thing we try to fix in agreement.
    if (numAxes < 4 || numAxes > 8 || numButtons < 8 || numHats > 1) {
        return QString();
    }

    char guidStr[33];
    SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(deviceIndex),
                              guidStr, sizeof(guidStr));

    const char* rawName = SDL_JoystickNameForIndex(deviceIndex);
    QString name = rawName ? QString::fromUtf8(rawName) : QStringLiteral("Unknown Gamepad");

    // Fields are comma separated, so a comma in the device name would split
    // the mapping and corrupt every field after it.
    name.replace(',', ' ');

    QStringList parts;
    parts << QString::fromLatin1(guidStr) << name;

    // Axis roles follow the same reasoning the Android client applies to the
    // ranges a device reports (see ControllerHandler): the first pair is
    // always the left stick, and whether the next pair is the right stick or
    // the triggers depends on how many axes exist.
    parts << QStringLiteral("leftx:a0") << QStringLiteral("lefty:a1");
    if (numAxes >= 6) {
        // X, Y, Z, Rx, Ry, Rz -- right stick on Rx/Ry, triggers on Z/Rz.
        parts << QStringLiteral("rightx:a3") << QStringLiteral("righty:a4");

        // Where a trigger axis rests decides its syntax. Announcing a
        // centre-resting axis as a whole axis makes SDL stretch
        // [-32768, 32767] onto [0, 32767], so the trigger reads ~50% pressed
        // at idle, forever -- and that is the common shape on exactly the
        // generic HID pads this fallback exists for. A trigger that idles at a
        // stop, on the other hand, needs its whole travel or it loses half of
        // it. The direction is assumed positive; a device that disagrees is
        // what the gamepad mapper is for.
        parts << QStringLiteral("lefttrigger:%1")
                 .arg(axisSourceToken(2, axisRestValues.value(2), 1));
        parts << QStringLiteral("righttrigger:%1")
                 .arg(axisSourceToken(5, axisRestValues.value(5), 1));
    }
    else {
        // X, Y, Z, Rz -- right stick on Z/Rz and no analog triggers. This is
        // the RadioMaster TX15 case.
        parts << QStringLiteral("rightx:a2") << QStringLiteral("righty:a3");
    }

    // Conventional HID gamepad button order. Devices with fewer buttons than
    // this simply get fewer bindings rather than wrong ones.
    static const char* const buttonTargets[] = {
        "a", "b", "x", "y",
        "leftshoulder", "rightshoulder",
        "back", "start",
        "leftstick", "rightstick",
        "guide"
    };
    for (int i = 0; i < numButtons && i < (int)SDL_arraysize(buttonTargets); i++) {
        parts << QStringLiteral("%1:b%2").arg(QLatin1String(buttonTargets[i])).arg(i);
    }

    if (numHats >= 1) {
        parts << QStringLiteral("dpup:h0.1") << QStringLiteral("dpright:h0.2")
              << QStringLiteral("dpdown:h0.4") << QStringLiteral("dpleft:h0.8");
    }

    // GUIDs differ between platforms for the same physical device, so a
    // mapping is only ever valid for the one that produced it.
    parts << QStringLiteral("platform:%1").arg(QLatin1String(SDL_GetPlatform()));

    return parts.join(QLatin1Char(','));
}

void MappingManager::applyFallbackMappings()
{
    s_GuessedDeviceNames.clear();
    s_GuessedDeviceGuids.clear();

    int numJoysticks = SDL_NumJoysticks();
    for (int i = 0; i < numJoysticks; i++) {
        // Anything SDL already recognises is left strictly alone.
        if (SDL_IsGameController(i)) {
            continue;
        }

        // The axis and button counts are only reachable through an open
        // handle. SDL refcounts opens, so this doesn't disturb a later
        // SDL_GameControllerOpen() by the caller.
        SDL_Joystick* joy = SDL_JoystickOpen(i);
        if (joy == nullptr) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Unable to open joystick %d to guess a mapping: %s",
                        i, SDL_GetError());
            continue;
        }

        // The device has to be sampled before it is closed: where each axis
        // rests is what decides whether a trigger gets whole-axis or half-axis
        // syntax, and the counts alone can't tell us.
        SDL_JoystickUpdate();

        int numAxes = SDL_JoystickNumAxes(joy);
        QVector<int> axisRestValues(numAxes);
        for (int axis = 0; axis < numAxes; axis++) {
            axisRestValues[axis] = SDL_JoystickGetAxis(joy, axis);
        }

        QString mapping = synthesizeMapping(i,
                                            numAxes,
                                            SDL_JoystickNumButtons(joy),
                                            SDL_JoystickNumHats(joy),
                                            axisRestValues);
        SDL_JoystickClose(joy);

        if (mapping.isEmpty()) {
            continue;
        }

        if (SDL_GameControllerAddMapping(qPrintable(mapping)) < 0) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Unable to add guessed mapping: %s",
                        SDL_GetError());
            continue;
        }

        const char* name = SDL_JoystickNameForIndex(i);
        s_GuessedDeviceNames.append(name ? QString::fromUtf8(name)
                                         : QStringLiteral("Unknown Gamepad"));

        char guessedGuidStr[33];
        SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(i),
                                  guessedGuidStr, sizeof(guessedGuidStr));
        s_GuessedDeviceGuids.append(QString::fromLatin1(guessedGuidStr));

        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Guessed a mapping for unrecognized gamepad: %s",
                    qPrintable(mapping));
    }
}

void MappingManager::addMapping(QString mappingString)
{
    SdlGamepadMapping mapping(mappingString);
    addMapping(mapping);
}

void MappingManager::addMapping(SdlGamepadMapping& mapping)
{
    m_Mappings[mapping.getGuid()] = mapping;
}

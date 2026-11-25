#pragma once

#include "mappingfetcher.h"

#include <QSettings>
#include <QSet>
#include <QStringList>
#include <QVector>

class SdlGamepadMapping
{
public:
    SdlGamepadMapping() {}

    SdlGamepadMapping(QString string)
    {
        QStringList mapping = string.split(",");
        if (!mapping.isEmpty()) {
            m_Guid = mapping[0];

            string.remove(0, m_Guid.length() + 1);
            m_Mapping = string;
        }
    }

    SdlGamepadMapping(QString guid, QString mapping)
        : m_Guid(guid),
          m_Mapping(mapping)
    {

    }

    bool operator==(const SdlGamepadMapping& other) const
    {
        return m_Guid == other.m_Guid && m_Mapping == other.m_Mapping;
    }

    QString getGuid() const
    {
        return m_Guid;
    }

    QString getMapping() const
    {
        return m_Mapping;
    }

    QString getSdlMappingString() const
    {
        if (m_Guid.isEmpty() || m_Mapping.isEmpty()) {
            return "";
        }
        else {
            return m_Guid + "," + m_Mapping;
        }
    }

private:
    QString m_Guid;
    QString m_Mapping;
};

class MappingManager
{
public:
    MappingManager();

    void addMapping(QString gamepadString);

    void addMapping(SdlGamepadMapping& gamepadMapping);

    void applyMappings();

    void save();

    // Names of the connected devices whose layout is a guess, as of the last
    // applyMappings(), in the order they were found. A guessed device is usable,
    // but not necessarily correct -- callers use this to offer the user a chance
    // to fix it. A guess made earlier in this process still counts: SDL then
    // knows the device as a game controller, but only through our guess. Empty
    // while guessing is turned off.
    static QStringList getGuessedDeviceNames();

    // The same devices by GUID. Names are not unique -- two controllers of the
    // same model are indistinguishable by name -- so this is what a caller
    // should match on.
    static QStringList getGuessedDeviceGuids();

    // Whether a device is a usable game controller: SDL's word, except for a
    // device whose only mapping is a guess made in this process while guessing
    // is now turned off. SDL cannot forget a mapping, so every place that picks
    // game controllers asks here, and turning the guess off takes effect at once.
    static bool isGameController(int deviceIndex);

    // A device SDL presents as a game controller only through a guess the user
    // has since turned off (see isGameController): to be left alone.
    static bool isIgnoredGuess(int deviceIndex);

    // How an axis should be written in a mapping, decided by where it rests
    // rather than by what it is being bound to. An axis idling against a stop
    // (a trigger reporting -32768 at rest) uses its whole travel; one idling
    // centered only uses the half it moves into. Getting this backwards costs
    // a trigger half its range, or leaves it reading half pressed at rest.
    // `deviation` only picks which half for the centered case.
    static QString axisSourceToken(int axis, int restValue, int deviation);

    // Builds an SDL mapping string for a joystick SDL has no entry for, using
    // its axis/button/hat counts and where its axes rest. Returns an empty
    // string for devices that don't look like gamepads. Pure: the device's GUID,
    // name and SDL's platform name are given.
    static QString synthesizeMapping(const QString& guid, const QString& name,
                                     int numAxes, int numButtons, int numHats,
                                     const QVector<int>& axisRestValues, const QString& platform);

private:
    // « Deviner la disposition des manettes inconnues » (ArtemisSettings), read
    // each time: mappingmanagersettings.cpp, apart so the unit test can set it
    static bool fallbackEnabled();

    void applyFallbackMappings();

    QMap<QString, SdlGamepadMapping> m_Mappings;

    static MappingFetcher* s_MappingFetcher;

    // GUIDs (lower case) given a guessed mapping in this process. Process-wide,
    // like SDL's own mappings: a guessed device stays a game controller to SDL
    // until the process ends, so it stays a guess here too -- until the user
    // saves a real mapping for it.
    static QSet<QString> s_GuessedGuids;
    // The connected ones, as of the last applyMappings()
    static QStringList s_GuessedDeviceNames;
    static QStringList s_GuessedDeviceGuids;
};


#pragma once

#include "mappingfetcher.h"

#include <QSettings>
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

    // Names of the devices applyMappings() had to guess a layout for, in the
    // order they were found. A guessed device is usable, but not necessarily
    // correct -- callers use this to offer the user a chance to fix it.
    static QStringList getGuessedDeviceNames();

    // The same devices by GUID. Names are not unique -- two controllers of the
    // same model are indistinguishable by name -- so this is what a caller
    // should match on.
    static QStringList getGuessedDeviceGuids();

    // How an axis should be written in a mapping, decided by where it rests
    // rather than by what it is being bound to. An axis idling against a stop
    // (a trigger reporting -32768 at rest) uses its whole travel; one idling
    // centered only uses the half it moves into. Getting this backwards costs
    // a trigger half its range, or leaves it reading half pressed at rest.
    // `deviation` only picks which half for the centered case.
    static QString axisSourceToken(int axis, int restValue, int deviation);

private:
    // Builds an SDL mapping string for a joystick SDL has no entry for, using
    // its axis/button/hat counts and where its axes rest. Returns an empty
    // string for devices that don't look like gamepads.
    static QString synthesizeMapping(int deviceIndex, int numAxes, int numButtons, int numHats,
                                     const QVector<int>& axisRestValues);

    void applyFallbackMappings();

    QMap<QString, SdlGamepadMapping> m_Mappings;

    static MappingFetcher* s_MappingFetcher;
    static QStringList s_GuessedDeviceNames;
    static QStringList s_GuessedDeviceGuids;
};


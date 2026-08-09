#pragma once

#include "mappingfetcher.h"

#include <QSettings>
#include <QStringList>

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

private:
    // Builds an SDL mapping string for a joystick SDL has no entry for, using
    // only its axis/button/hat counts. Returns an empty string for devices
    // that don't look like gamepads.
    static QString synthesizeMapping(int deviceIndex, int numAxes, int numButtons, int numHats);

    void applyFallbackMappings();

    QMap<QString, SdlGamepadMapping> m_Mappings;

    static MappingFetcher* s_MappingFetcher;
    static QStringList s_GuessedDeviceNames;
};


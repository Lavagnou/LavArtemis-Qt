#include "mappingmanager.h"
#include "artemissettings.h"

// Apart from mappingmanager.cpp so that its unit test (tests/mapping) links without the settings
bool MappingManager::fallbackEnabled()
{
    return ArtemisSettings::instance()->genericGamepadFallbackEnabled();
}

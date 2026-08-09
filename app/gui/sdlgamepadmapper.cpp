#include "sdlgamepadmapper.h"

#include "settings/mappingmanager.h"

#include <QCoreApplication>

#define POLLING_INTERVAL_MS 20

// An axis has to travel this far from where it was resting before we accept
// it. Sticks rest centered, but switches and triggers can rest at an extreme,
// so an absolute threshold would either miss them or fire instantly.
#define AXIS_DEVIATION_THRESHOLD 16384

namespace {

struct MappingStep {
    const char* element;
    const char* prompt;
};

// Order matters: it is what the user is walked through. Face buttons first
// because they are unambiguous, sticks last because they take the longest to
// explain.
const MappingStep k_Steps[] = {
    { "a",             QT_TRANSLATE_NOOP("SdlGamepadMapper", "Press the bottom face button (A / Cross)") },
    { "b",             QT_TRANSLATE_NOOP("SdlGamepadMapper", "Press the right face button (B / Circle)") },
    { "x",             QT_TRANSLATE_NOOP("SdlGamepadMapper", "Press the left face button (X / Square)") },
    { "y",             QT_TRANSLATE_NOOP("SdlGamepadMapper", "Press the top face button (Y / Triangle)") },
    { "leftshoulder",  QT_TRANSLATE_NOOP("SdlGamepadMapper", "Press the left shoulder button (LB / L1)") },
    { "rightshoulder", QT_TRANSLATE_NOOP("SdlGamepadMapper", "Press the right shoulder button (RB / R1)") },
    { "lefttrigger",   QT_TRANSLATE_NOOP("SdlGamepadMapper", "Pull the left trigger (LT / L2)") },
    { "righttrigger",  QT_TRANSLATE_NOOP("SdlGamepadMapper", "Pull the right trigger (RT / R2)") },
    { "back",          QT_TRANSLATE_NOOP("SdlGamepadMapper", "Press Back / Select / View") },
    { "start",         QT_TRANSLATE_NOOP("SdlGamepadMapper", "Press Start / Options / Menu") },
    { "guide",         QT_TRANSLATE_NOOP("SdlGamepadMapper", "Press the Guide / Home button") },
    { "leftstick",     QT_TRANSLATE_NOOP("SdlGamepadMapper", "Press the left stick in") },
    { "rightstick",    QT_TRANSLATE_NOOP("SdlGamepadMapper", "Press the right stick in") },
    { "dpup",          QT_TRANSLATE_NOOP("SdlGamepadMapper", "Press D-pad up") },
    { "dpdown",        QT_TRANSLATE_NOOP("SdlGamepadMapper", "Press D-pad down") },
    { "dpleft",        QT_TRANSLATE_NOOP("SdlGamepadMapper", "Press D-pad left") },
    { "dpright",       QT_TRANSLATE_NOOP("SdlGamepadMapper", "Press D-pad right") },
    { "leftx",         QT_TRANSLATE_NOOP("SdlGamepadMapper", "Move the left stick RIGHT") },
    { "lefty",         QT_TRANSLATE_NOOP("SdlGamepadMapper", "Move the left stick DOWN") },
    { "rightx",        QT_TRANSLATE_NOOP("SdlGamepadMapper", "Move the right stick RIGHT") },
    { "righty",        QT_TRANSLATE_NOOP("SdlGamepadMapper", "Move the right stick DOWN") },
};

bool isAxisElement(const QString& element)
{
    return element == QLatin1String("leftx") || element == QLatin1String("lefty") ||
           element == QLatin1String("rightx") || element == QLatin1String("righty");
}

}

SdlGamepadMapper* SdlGamepadMapper::s_instance;

SdlGamepadMapper* SdlGamepadMapper::instance()
{
    if (!s_instance) {
        s_instance = new SdlGamepadMapper();
    }
    return s_instance;
}

SdlGamepadMapper* SdlGamepadMapper::create(QQmlEngine* qmlEngine, QJSEngine* jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    return instance();
}

SdlGamepadMapper::SdlGamepadMapper(QObject* parent)
    : QObject(parent),
      m_Joystick(nullptr),
      m_CurrentStep(0)
{
    m_PollingTimer = new QTimer(this);
    m_PollingTimer->setInterval(POLLING_INTERVAL_MS);
    connect(m_PollingTimer, &QTimer::timeout, this, &SdlGamepadMapper::onPollingTimerFired);
}

SdlGamepadMapper::~SdlGamepadMapper()
{
    closeDevice();
}

int SdlGamepadMapper::stepCount() const
{
    return (int)SDL_arraysize(k_Steps);
}

QString SdlGamepadMapper::currentElement() const
{
    if (m_CurrentStep < 0 || m_CurrentStep >= stepCount()) {
        return QString();
    }
    return QLatin1String(k_Steps[m_CurrentStep].element);
}

QString SdlGamepadMapper::currentPrompt() const
{
    if (m_CurrentStep < 0 || m_CurrentStep >= stepCount()) {
        return QString();
    }
    return QCoreApplication::translate("SdlGamepadMapper", k_Steps[m_CurrentStep].prompt);
}

QVariantList SdlGamepadMapper::enumerateDevices()
{
    QVariantList devices;

    // The joystick subsystem is what enumerates devices; the caller may not
    // have it up yet if no gamepad navigation is active.
    bool weInitialized = !SDL_WasInit(SDL_INIT_JOYSTICK);
    if (weInitialized && SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_JOYSTICK) failed: %s",
                     SDL_GetError());
        return devices;
    }

    SDL_JoystickUpdate();

    QStringList guessed = MappingManager::getGuessedDeviceNames();

    int numJoysticks = SDL_NumJoysticks();
    for (int i = 0; i < numJoysticks; i++) {
        char guidStr[33];
        SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(i), guidStr, sizeof(guidStr));

        const char* rawName = SDL_JoystickNameForIndex(i);
        QString name = rawName ? QString::fromUtf8(rawName) : QStringLiteral("Unknown device");

        QVariantMap device;
        device[QStringLiteral("index")] = i;
        device[QStringLiteral("name")] = name;
        device[QStringLiteral("guid")] = QString::fromLatin1(guidStr);
        device[QStringLiteral("recognized")] = SDL_IsGameController(i) == SDL_TRUE;
        device[QStringLiteral("guessed")] = guessed.contains(name);

        SDL_Joystick* joy = SDL_JoystickOpen(i);
        if (joy != nullptr) {
            device[QStringLiteral("numAxes")] = SDL_JoystickNumAxes(joy);
            device[QStringLiteral("numButtons")] = SDL_JoystickNumButtons(joy);
            device[QStringLiteral("numHats")] = SDL_JoystickNumHats(joy);
            SDL_JoystickClose(joy);
        }
        else {
            device[QStringLiteral("numAxes")] = 0;
            device[QStringLiteral("numButtons")] = 0;
            device[QStringLiteral("numHats")] = 0;
        }

        devices.append(device);
    }

    if (weInitialized) {
        SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    }

    return devices;
}

void SdlGamepadMapper::startMapping(int deviceIndex)
{
    closeDevice();

    if (!SDL_WasInit(SDL_INIT_JOYSTICK) && SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_JOYSTICK) failed: %s",
                     SDL_GetError());
        return;
    }

    m_Joystick = SDL_JoystickOpen(deviceIndex);
    if (m_Joystick == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_JoystickOpen(%d) failed: %s",
                     deviceIndex, SDL_GetError());
        return;
    }

    char guidStr[33];
    SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(m_Joystick), guidStr, sizeof(guidStr));
    m_DeviceGuid = QString::fromLatin1(guidStr);

    const char* rawName = SDL_JoystickName(m_Joystick);
    m_DeviceName = rawName ? QString::fromUtf8(rawName) : QStringLiteral("Unknown device");
    // Commas separate fields in an SDL mapping, so one in the name would
    // corrupt every field after it.
    m_DeviceName.replace(',', ' ');

    m_Bindings.clear();
    m_CurrentStep = 0;

    // Sample where everything rests right now. Anything already deflected
    // stays "at rest" as far as we're concerned, which is what lets a switch
    // that idles at full scale be mapped at all.
    SDL_JoystickUpdate();

    int numAxes = SDL_JoystickNumAxes(m_Joystick);
    m_AxisRestValues.resize(numAxes);
    for (int i = 0; i < numAxes; i++) {
        m_AxisRestValues[i] = SDL_JoystickGetAxis(m_Joystick, i);
    }

    int numButtons = SDL_JoystickNumButtons(m_Joystick);
    m_ButtonWasDown.resize(numButtons);
    for (int i = 0; i < numButtons; i++) {
        m_ButtonWasDown[i] = SDL_JoystickGetButton(m_Joystick, i) != 0;
    }

    m_PollingTimer->start();

    emit mappingChanged();
    emit currentStepChanged();
}

void SdlGamepadMapper::skipCurrentStep()
{
    if (m_Joystick == nullptr) {
        return;
    }
    advanceStep();
}

void SdlGamepadMapper::restart()
{
    if (m_Joystick == nullptr) {
        return;
    }
    m_Bindings.clear();
    m_CurrentStep = 0;
    emit currentStepChanged();
}

void SdlGamepadMapper::cancel()
{
    closeDevice();
    m_Bindings.clear();
    m_CurrentStep = 0;
    emit mappingChanged();
    emit currentStepChanged();
}

void SdlGamepadMapper::closeDevice()
{
    m_PollingTimer->stop();

    if (m_Joystick != nullptr) {
        SDL_JoystickClose(m_Joystick);
        m_Joystick = nullptr;
    }
}

QString SdlGamepadMapper::previewMapping() const
{
    if (m_DeviceGuid.isEmpty() || m_Bindings.isEmpty()) {
        return QString();
    }

    QStringList parts;
    parts << m_DeviceGuid << m_DeviceName;

    // Emit in the wizard's order rather than the map's, so the preview reads
    // the same way the user filled it in.
    for (int i = 0; i < stepCount(); i++) {
        QString element = QLatin1String(k_Steps[i].element);
        auto it = m_Bindings.constFind(element);
        if (it != m_Bindings.constEnd()) {
            parts << QStringLiteral("%1:%2").arg(element, it.value());
        }
    }

    // A GUID identifies a device differently on each OS, so a mapping is only
    // valid on the platform that produced it.
    parts << QStringLiteral("platform:%1").arg(QLatin1String(SDL_GetPlatform()));

    return parts.join(QLatin1Char(','));
}

bool SdlGamepadMapper::saveMapping()
{
    QString mapping = previewMapping();
    if (mapping.isEmpty()) {
        return false;
    }

    MappingManager mappingManager;
    mappingManager.addMapping(mapping);
    mappingManager.save();
    mappingManager.applyMappings();

    closeDevice();
    emit mappingChanged();

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Saved user gamepad mapping: %s",
                qPrintable(mapping));

    return true;
}

void SdlGamepadMapper::advanceStep()
{
    m_CurrentStep++;

    if (m_CurrentStep >= stepCount()) {
        m_PollingTimer->stop();
        emit currentStepChanged();
        emit mappingComplete();
        return;
    }

    emit currentStepChanged();
}

void SdlGamepadMapper::captureBinding(const QString& binding)
{
    QString element = currentElement();
    if (element.isEmpty()) {
        return;
    }

    m_Bindings[element] = binding;
    emit inputCaptured(binding);

    advanceStep();
}

bool SdlGamepadMapper::pollForInput(QString& binding)
{
    bool wantAxis = isAxisElement(currentElement());

    // Buttons first: they're unambiguous, and a stick pressed in while being
    // moved shouldn't be read as an axis.
    int numButtons = SDL_JoystickNumButtons(m_Joystick);
    for (int i = 0; i < numButtons && i < m_ButtonWasDown.size(); i++) {
        bool down = SDL_JoystickGetButton(m_Joystick, i) != 0;

        if (!down) {
            m_ButtonWasDown[i] = false;
            continue;
        }

        // Held since before this step started: wait for a fresh press.
        if (m_ButtonWasDown[i]) {
            continue;
        }

        m_ButtonWasDown[i] = true;

        if (!wantAxis) {
            binding = QStringLiteral("b%1").arg(i);
            return true;
        }
    }

    int numHats = SDL_JoystickNumHats(m_Joystick);
    for (int i = 0; i < numHats && !wantAxis; i++) {
        Uint8 hat = SDL_JoystickGetHat(m_Joystick, i);
        if (hat != SDL_HAT_CENTERED) {
            binding = QStringLiteral("h%1.%2").arg(i).arg(hat);
            return true;
        }
    }

    int numAxes = SDL_JoystickNumAxes(m_Joystick);
    for (int i = 0; i < numAxes && i < m_AxisRestValues.size(); i++) {
        int value = SDL_JoystickGetAxis(m_Joystick, i);
        int deviation = value - m_AxisRestValues[i];

        if (qAbs(deviation) < AXIS_DEVIATION_THRESHOLD) {
            continue;
        }

        if (wantAxis) {
            // The prompt asks for a specific direction (right, down). If the
            // axis went the other way, the device reports it inverted and SDL
            // needs to know, or the stick will fight the user in game.
            binding = (deviation < 0) ? QStringLiteral("a%1~").arg(i)
                                      : QStringLiteral("a%1").arg(i);
        }
        else {
            // A trigger or a switch on an axis. Half-axis syntax keeps the
            // resting half from counting as "pressed".
            binding = (deviation < 0) ? QStringLiteral("-a%1").arg(i)
                                      : QStringLiteral("+a%1").arg(i);
        }
        return true;
    }

    return false;
}

void SdlGamepadMapper::onPollingTimerFired()
{
    if (m_Joystick == nullptr) {
        return;
    }

    // Update joystick state without pumping other events: video events in
    // particular must not be consumed from under the QML window.
    SDL_JoystickUpdate();

    if (!SDL_JoystickGetAttached(m_Joystick)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Device disappeared while mapping");
        cancel();
        return;
    }

    QString binding;
    if (pollForInput(binding)) {
        captureBinding(binding);
    }
}

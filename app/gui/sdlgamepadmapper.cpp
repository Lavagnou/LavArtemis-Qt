#include "sdlgamepadmapper.h"

#include "settings/mappingmanager.h"

#include <QCoreApplication>

#define POLLING_INTERVAL_MS 20

// An axis has to travel this far from where it was resting before we accept
// it. Sticks rest centered, but switches and triggers can rest at an extreme,
// so an absolute threshold would either miss them or fire instantly.
#define AXIS_DEVIATION_THRESHOLD 16384

// Movement worth naming in the raw activity readout. Lower than the capture
// threshold on purpose: it should show the device is alive well before
// anything is committed.
#define AXIS_ACTIVITY_THRESHOLD 6000

namespace {

struct MappingElement {
    const char* element;
    const char* label;
    const char* kind;       // "button", "trigger" or "axis"
    const char* prompt;
};

// Order matters: it is the order "map everything" walks through. Face buttons
// first because they are unambiguous, sticks last because they take the
// longest to explain.
const MappingElement k_Elements[] = {
    { "a",             QT_TRANSLATE_NOOP("SdlGamepadMapper", "A"),  "button",
      QT_TRANSLATE_NOOP("SdlGamepadMapper", "Press the bottom face button (A / Cross)") },
    { "b",             QT_TRANSLATE_NOOP("SdlGamepadMapper", "B"),  "button",
      QT_TRANSLATE_NOOP("SdlGamepadMapper", "Press the right face button (B / Circle)") },
    { "x",             QT_TRANSLATE_NOOP("SdlGamepadMapper", "X"),  "button",
      QT_TRANSLATE_NOOP("SdlGamepadMapper", "Press the left face button (X / Square)") },
    { "y",             QT_TRANSLATE_NOOP("SdlGamepadMapper", "Y"),  "button",
      QT_TRANSLATE_NOOP("SdlGamepadMapper", "Press the top face button (Y / Triangle)") },
    { "leftshoulder",  QT_TRANSLATE_NOOP("SdlGamepadMapper", "LB"), "button",
      QT_TRANSLATE_NOOP("SdlGamepadMapper", "Press the left shoulder button (LB / L1)") },
    { "rightshoulder", QT_TRANSLATE_NOOP("SdlGamepadMapper", "RB"), "button",
      QT_TRANSLATE_NOOP("SdlGamepadMapper", "Press the right shoulder button (RB / R1)") },
    { "lefttrigger",   QT_TRANSLATE_NOOP("SdlGamepadMapper", "LT"), "trigger",
      QT_TRANSLATE_NOOP("SdlGamepadMapper", "Pull the left trigger (LT / L2) all the way") },
    { "righttrigger",  QT_TRANSLATE_NOOP("SdlGamepadMapper", "RT"), "trigger",
      QT_TRANSLATE_NOOP("SdlGamepadMapper", "Pull the right trigger (RT / R2) all the way") },
    { "back",          QT_TRANSLATE_NOOP("SdlGamepadMapper", "Back"),  "button",
      QT_TRANSLATE_NOOP("SdlGamepadMapper", "Press Back / Select / View") },
    { "start",         QT_TRANSLATE_NOOP("SdlGamepadMapper", "Start"), "button",
      QT_TRANSLATE_NOOP("SdlGamepadMapper", "Press Start / Options / Menu") },
    { "guide",         QT_TRANSLATE_NOOP("SdlGamepadMapper", "Guide"), "button",
      QT_TRANSLATE_NOOP("SdlGamepadMapper", "Press the Guide / Home button") },
    { "leftstick",     QT_TRANSLATE_NOOP("SdlGamepadMapper", "L3"), "button",
      QT_TRANSLATE_NOOP("SdlGamepadMapper", "Press the left stick in") },
    { "rightstick",    QT_TRANSLATE_NOOP("SdlGamepadMapper", "R3"), "button",
      QT_TRANSLATE_NOOP("SdlGamepadMapper", "Press the right stick in") },
    { "dpup",          QT_TRANSLATE_NOOP("SdlGamepadMapper", "D-pad up"),    "button",
      QT_TRANSLATE_NOOP("SdlGamepadMapper", "Press D-pad up") },
    { "dpdown",        QT_TRANSLATE_NOOP("SdlGamepadMapper", "D-pad down"),  "button",
      QT_TRANSLATE_NOOP("SdlGamepadMapper", "Press D-pad down") },
    { "dpleft",        QT_TRANSLATE_NOOP("SdlGamepadMapper", "D-pad left"),  "button",
      QT_TRANSLATE_NOOP("SdlGamepadMapper", "Press D-pad left") },
    { "dpright",       QT_TRANSLATE_NOOP("SdlGamepadMapper", "D-pad right"), "button",
      QT_TRANSLATE_NOOP("SdlGamepadMapper", "Press D-pad right") },
    { "leftx",         QT_TRANSLATE_NOOP("SdlGamepadMapper", "Left stick X"),  "axis",
      QT_TRANSLATE_NOOP("SdlGamepadMapper", "Move the LEFT stick RIGHT") },
    { "lefty",         QT_TRANSLATE_NOOP("SdlGamepadMapper", "Left stick Y"),  "axis",
      QT_TRANSLATE_NOOP("SdlGamepadMapper", "Move the LEFT stick DOWN") },
    { "rightx",        QT_TRANSLATE_NOOP("SdlGamepadMapper", "Right stick X"), "axis",
      QT_TRANSLATE_NOOP("SdlGamepadMapper", "Move the RIGHT stick RIGHT") },
    { "righty",        QT_TRANSLATE_NOOP("SdlGamepadMapper", "Right stick Y"), "axis",
      QT_TRANSLATE_NOOP("SdlGamepadMapper", "Move the RIGHT stick DOWN") },
};

const MappingElement* findElement(const QString& element)
{
    for (int i = 0; i < (int)SDL_arraysize(k_Elements); i++) {
        if (element == QLatin1String(k_Elements[i].element)) {
            return &k_Elements[i];
        }
    }
    return nullptr;
}

QString elementKind(const QString& element)
{
    const MappingElement* entry = findElement(element);
    return entry ? QLatin1String(entry->kind) : QLatin1String("button");
}

bool isSignedAxisElement(const QString& element)
{
    return elementKind(element) == QLatin1String("axis");
}

// The element a mapping field describes, with any half-axis output prefix
// removed. "crc" and "platform" come back as themselves, which is what keeps
// them out of the binding map.
QString fieldElement(const QString& field)
{
    int separator = field.indexOf(QLatin1Char(':'));
    QString element = separator < 0 ? field : field.left(separator);
    while (!element.isEmpty() &&
           (element.at(0) == QLatin1Char('+') || element.at(0) == QLatin1Char('-'))) {
        element.remove(0, 1);
    }
    return element;
}

// What a source token reads right now, scaled the way SDL would scale it.
double evaluateSource(SDL_Joystick* joystick, const QString& source, const QString& kind)
{
    if (joystick == nullptr || source.isEmpty()) {
        return 0.0;
    }

    if (source.startsWith(QLatin1Char('b'))) {
        return SDL_JoystickGetButton(joystick, source.mid(1).toInt()) != 0 ? 1.0 : 0.0;
    }

    if (source.startsWith(QLatin1Char('h'))) {
        int dot = source.indexOf(QLatin1Char('.'));
        if (dot < 0) {
            return 0.0;
        }
        int mask = source.mid(dot + 1).toInt();
        if (mask == 0) {
            return 0.0;
        }
        Uint8 hat = SDL_JoystickGetHat(joystick, source.mid(1, dot - 1).toInt());
        return (hat & mask) == mask ? 1.0 : 0.0;
    }

    QString token = source;
    QChar half;
    if (token.startsWith(QLatin1Char('+')) || token.startsWith(QLatin1Char('-'))) {
        half = token.at(0);
        token.remove(0, 1);
    }

    bool inverted = token.endsWith(QLatin1Char('~'));
    if (inverted) {
        token.chop(1);
    }

    if (!token.startsWith(QLatin1Char('a'))) {
        return 0.0;
    }

    double value = SDL_JoystickGetAxis(joystick, token.mid(1).toInt());
    if (inverted) {
        value = -value;
    }

    if (half == QLatin1Char('+')) {
        return qBound(0.0, value / 32767.0, 1.0);
    }
    if (half == QLatin1Char('-')) {
        return qBound(0.0, -value / 32767.0, 1.0);
    }

    if (kind == QLatin1String("axis")) {
        return qBound(-1.0, value / 32767.0, 1.0);
    }

    // A whole axis driving a trigger or a button: SDL stretches
    // [-32768, 32767] onto [0, 32767]. That is exactly why an axis resting
    // centered reads half pressed forever, and showing it is the point.
    return qBound(0.0, (value + 32768.0) / 65535.0, 1.0);
}

QString describeHatDirection(Uint8 hat)
{
    switch (hat) {
    case SDL_HAT_UP:    return QCoreApplication::translate("SdlGamepadMapper", "up");
    case SDL_HAT_RIGHT: return QCoreApplication::translate("SdlGamepadMapper", "right");
    case SDL_HAT_DOWN:  return QCoreApplication::translate("SdlGamepadMapper", "down");
    case SDL_HAT_LEFT:  return QCoreApplication::translate("SdlGamepadMapper", "left");
    default:            return QCoreApplication::translate("SdlGamepadMapper", "diagonal");
    }
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
      m_Joystick(nullptr)
{
    m_PollingTimer = new QTimer(this);
    m_PollingTimer->setInterval(POLLING_INTERVAL_MS);
    connect(m_PollingTimer, &QTimer::timeout, this, &SdlGamepadMapper::onPollingTimerFired);
}

SdlGamepadMapper::~SdlGamepadMapper()
{
    closeDevice();
}

QVariantList SdlGamepadMapper::elementCatalog() const
{
    QVariantList catalog;

    for (int i = 0; i < (int)SDL_arraysize(k_Elements); i++) {
        QVariantMap entry;
        entry[QStringLiteral("element")] = QLatin1String(k_Elements[i].element);
        entry[QStringLiteral("label")] =
                QCoreApplication::translate("SdlGamepadMapper", k_Elements[i].label);
        entry[QStringLiteral("kind")] = QLatin1String(k_Elements[i].kind);
        catalog.append(entry);
    }

    return catalog;
}

QString SdlGamepadMapper::targetPrompt() const
{
    const MappingElement* entry = findElement(m_TargetElement);
    if (entry == nullptr) {
        return QString();
    }
    return QCoreApplication::translate("SdlGamepadMapper", entry->prompt);
}

QVariantMap SdlGamepadMapper::bindings() const
{
    QVariantMap map;
    for (auto it = m_Bindings.constBegin(); it != m_Bindings.constEnd(); ++it) {
        map.insert(it.key(), it.value());
    }
    return map;
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

    // Both badges this list shows are side effects of applying the mappings.
    // Reading the guessed list without doing this first reported a guessed
    // device as "Recognized" whenever the mapper was opened before anything
    // else had run -- the exact opposite of what this screen exists to say.
    MappingManager mappingManager;
    mappingManager.applyMappings();

    QStringList guessedGuids = MappingManager::getGuessedDeviceGuids();

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
        device[QStringLiteral("guessed")] = guessedGuids.contains(QString::fromLatin1(guidStr));

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
    m_PassthroughFields.clear();
    m_TargetElement.clear();
    m_TargetQueue.clear();
    m_ElementValues.clear();
    m_RawActivity.clear();

    // Sample where everything rests right now. Anything already deflected
    // stays "at rest" as far as we're concerned, which is what lets a switch
    // that idles at full scale be mapped at all.
    SDL_JoystickUpdate();

    int numAxes = SDL_JoystickNumAxes(m_Joystick);
    m_AxisRestValues.resize(numAxes);
    for (int i = 0; i < numAxes; i++) {
        m_AxisRestValues[i] = SDL_JoystickGetAxis(m_Joystick, i);
    }

    // Start from whatever SDL already knows about this device -- a database
    // entry or a guess -- so one wrong stick can be corrected on its own
    // instead of forcing all 21 controls to be redone.
    loadExistingMapping();

    beginListening();

    // Runs for as long as the device is open, not just during a capture: it
    // also feeds the live readout that shows what each binding is doing.
    m_PollingTimer->start();

    emit mappingChanged();
    emit targetChanged();
    emit bindingsChanged();
    emit liveStateChanged();
}

void SdlGamepadMapper::loadExistingMapping()
{
    char* rawMapping = SDL_GameControllerMappingForGUID(SDL_JoystickGetGUID(m_Joystick));
    if (rawMapping == nullptr) {
        return;
    }

    QString mapping = QString::fromUtf8(rawMapping);
    SDL_free(rawMapping);

    // Field 0 is the GUID and field 1 the name; neither is a binding.
    const QStringList fields = mapping.split(QLatin1Char(','));
    for (int i = 2; i < fields.count(); i++) {
        const QString& field = fields.at(i);
        int separator = field.indexOf(QLatin1Char(':'));
        if (separator <= 0 || separator == field.length() - 1) {
            continue;
        }

        QString element = fieldElement(field);
        if (element == QLatin1String("platform")) {
            // Re-derived on save, for the platform we are actually running on.
            continue;
        }
        if (element == QLatin1String("crc")) {
            // Deliberately dropped rather than carried over. A crc field
            // disambiguates devices that share a GUID, and the one in a
            // database entry belongs to that entry's device name, not
            // necessarily to this one. Our GUID comes straight off the open
            // joystick, so SDL can match it without help -- and a wrong crc
            // would make the saved mapping silently fail to apply.
            continue;
        }

        // A half-axis output ("+lefty:b11") describes half an element, which
        // this class has no way to represent. Keep such a field verbatim
        // rather than collapsing it onto the whole element and silently
        // changing what the device does.
        bool wholeElement = (field.left(separator) == element);
        if (wholeElement && findElement(element) != nullptr) {
            m_Bindings.insert(element, field.mid(separator + 1));
        }
        else {
            m_PassthroughFields.append(field);
        }
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Loaded %d existing binding(s) and %d passthrough field(s) for %s",
                m_Bindings.count(), m_PassthroughFields.count(), qPrintable(m_DeviceGuid));
}

void SdlGamepadMapper::selectElement(const QString& element)
{
    selectElements(QStringList() << element);
}

void SdlGamepadMapper::selectElements(const QStringList& elements)
{
    if (m_Joystick == nullptr) {
        return;
    }

    m_TargetQueue.clear();
    for (const QString& element : elements) {
        if (findElement(element) != nullptr) {
            m_TargetQueue.append(element);
        }
    }

    if (m_TargetQueue.isEmpty()) {
        cancelSelection();
        return;
    }

    m_TargetElement = m_TargetQueue.takeFirst();
    beginListening();
    emit targetChanged();
}

void SdlGamepadMapper::mapEverything()
{
    QStringList everything;
    for (int i = 0; i < (int)SDL_arraysize(k_Elements); i++) {
        everything.append(QLatin1String(k_Elements[i].element));
    }
    selectElements(everything);
}

void SdlGamepadMapper::skipTarget()
{
    if (m_Joystick == nullptr || m_TargetElement.isEmpty()) {
        return;
    }
    advanceQueue();
}

void SdlGamepadMapper::cancelSelection()
{
    m_TargetQueue.clear();
    if (m_TargetElement.isEmpty()) {
        return;
    }
    m_TargetElement.clear();
    emit targetChanged();
}

void SdlGamepadMapper::clearElement(const QString& element)
{
    if (m_Bindings.remove(element) == 0) {
        return;
    }
    m_ElementValues.remove(element);
    emit bindingsChanged();
    emit liveStateChanged();
}

void SdlGamepadMapper::clearAll()
{
    if (m_Bindings.isEmpty() && m_PassthroughFields.isEmpty()) {
        return;
    }
    m_Bindings.clear();
    m_PassthroughFields.clear();
    m_ElementValues.clear();
    emit bindingsChanged();
    emit liveStateChanged();
}

void SdlGamepadMapper::cancel()
{
    closeDevice();
    m_Bindings.clear();
    m_PassthroughFields.clear();
    m_TargetElement.clear();
    m_TargetQueue.clear();
    m_ElementValues.clear();
    m_RawActivity.clear();
    emit mappingChanged();
    emit targetChanged();
    emit bindingsChanged();
    emit liveStateChanged();
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

    // Emit in catalog order rather than the map's, so the preview reads the
    // way the controller is laid out.
    for (int i = 0; i < (int)SDL_arraysize(k_Elements); i++) {
        QString element = QLatin1String(k_Elements[i].element);
        auto it = m_Bindings.constFind(element);
        if (it != m_Bindings.constEnd()) {
            parts << QStringLiteral("%1:%2").arg(element, it.value());
        }
    }

    // Whatever the device's previous mapping had that we don't model, unless
    // its element has since been bound here -- two sources for one control
    // would leave SDL picking one of them at random.
    for (const QString& field : m_PassthroughFields) {
        if (m_Bindings.contains(fieldElement(field))) {
            continue;
        }
        parts << field;
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

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Saved user gamepad mapping: %s",
                qPrintable(mapping));

    return true;
}

void SdlGamepadMapper::beginListening()
{
    if (m_Joystick == nullptr) {
        return;
    }

    SDL_JoystickUpdate();

    int numAxes = SDL_JoystickNumAxes(m_Joystick);
    m_AxisRestValues.resize(numAxes);
    m_AxisArmed.resize(numAxes);
    for (int i = 0; i < numAxes; i++) {
        int value = SDL_JoystickGetAxis(m_Joystick, i);
        int deviation = value - m_AxisRestValues[i];

        if (qAbs(deviation) < AXIS_DEVIATION_THRESHOLD) {
            // Quiet. Take this as the new rest, so a centre that has drifted
            // since the device was opened doesn't fire on every capture.
            m_AxisRestValues[i] = (Sint16)value;
            m_AxisArmed[i] = true;
        }
        else {
            // Being held right now: not a new rest, and not eligible until it
            // has gone back to the old one.
            m_AxisArmed[i] = false;
        }
    }

    int numButtons = SDL_JoystickNumButtons(m_Joystick);
    m_ButtonArmed.resize(numButtons);
    for (int i = 0; i < numButtons; i++) {
        m_ButtonArmed[i] = SDL_JoystickGetButton(m_Joystick, i) == 0;
    }

    int numHats = SDL_JoystickNumHats(m_Joystick);
    m_HatArmed.resize(numHats);
    for (int i = 0; i < numHats; i++) {
        m_HatArmed[i] = SDL_JoystickGetHat(m_Joystick, i) == SDL_HAT_CENTERED;
    }
}

void SdlGamepadMapper::advanceQueue()
{
    if (m_TargetQueue.isEmpty()) {
        m_TargetElement.clear();
        emit targetChanged();
        emit queueFinished();
        return;
    }

    m_TargetElement = m_TargetQueue.takeFirst();
    beginListening();
    emit targetChanged();
}

QString SdlGamepadMapper::axisToken(int axis, int deviation, bool signedAxis) const
{
    if (signedAxis) {
        // The prompt asks for a specific direction (right, down). If the axis
        // went the other way the device reports it inverted and SDL needs to
        // know, or the stick will fight the user in game.
        return (deviation < 0) ? QStringLiteral("a%1~").arg(axis)
                               : QStringLiteral("a%1").arg(axis);
    }

    // A trigger or a switch on an axis: where it rests decides the syntax, not
    // what kind of element it is being bound to. Same rule the fallback
    // guesser applies, and deliberately the same code -- a device mapped by
    // hand and the same device guessed at should not disagree about how its
    // triggers work.
    return MappingManager::axisSourceToken(axis, m_AxisRestValues[axis], deviation);
}

bool SdlGamepadMapper::captureFromState(QString& binding)
{
    bool signedAxis = isSignedAxisElement(m_TargetElement);

    // Buttons first: they're unambiguous, and a stick pressed in while being
    // moved shouldn't be read as an axis.
    int numButtons = qMin(SDL_JoystickNumButtons(m_Joystick), (int)m_ButtonArmed.size());
    for (int i = 0; i < numButtons; i++) {
        if (SDL_JoystickGetButton(m_Joystick, i) == 0) {
            m_ButtonArmed[i] = true;
            continue;
        }

        // Held since before we started listening: wait for a fresh press.
        if (!m_ButtonArmed[i]) {
            continue;
        }

        // Either way this press is spent. Disarming it even when we can't use
        // it is what stops one press from also satisfying the next target.
        m_ButtonArmed[i] = false;

        if (!signedAxis) {
            binding = QStringLiteral("b%1").arg(i);
            return true;
        }
    }

    int numHats = qMin(SDL_JoystickNumHats(m_Joystick), (int)m_HatArmed.size());
    for (int i = 0; i < numHats; i++) {
        Uint8 hat = SDL_JoystickGetHat(m_Joystick, i);

        if (hat == SDL_HAT_CENTERED) {
            m_HatArmed[i] = true;
            continue;
        }

        if (!m_HatArmed[i]) {
            continue;
        }

        // A diagonal is a D-pad caught mid-roll. SDL matches a hat with
        // (value & mask) == mask, so recording h0.3 would give a direction
        // that only ever fires while two are held. Stay armed and wait for it
        // to settle on one.
        if (hat != SDL_HAT_UP && hat != SDL_HAT_RIGHT &&
            hat != SDL_HAT_DOWN && hat != SDL_HAT_LEFT) {
            continue;
        }

        m_HatArmed[i] = false;

        if (!signedAxis) {
            binding = QStringLiteral("h%1.%2").arg(i).arg(hat);
            return true;
        }
    }

    // Largest deviation wins rather than lowest index: on a device with one
    // drifting axis, first-index-wins handed that axis every element.
    int numAxes = qMin(SDL_JoystickNumAxes(m_Joystick), (int)m_AxisArmed.size());
    int bestAxis = -1;
    int bestDeviation = 0;
    for (int i = 0; i < numAxes; i++) {
        int deviation = SDL_JoystickGetAxis(m_Joystick, i) - m_AxisRestValues[i];

        if (qAbs(deviation) < AXIS_DEVIATION_THRESHOLD) {
            m_AxisArmed[i] = true;
            continue;
        }

        if (!m_AxisArmed[i]) {
            continue;
        }

        if (bestAxis < 0 || qAbs(deviation) > qAbs(bestDeviation)) {
            bestAxis = i;
            bestDeviation = deviation;
        }
    }

    if (bestAxis < 0) {
        return false;
    }

    m_AxisArmed[bestAxis] = false;
    binding = axisToken(bestAxis, bestDeviation, signedAxis);
    return true;
}

void SdlGamepadMapper::refreshLiveState()
{
    QVariantMap values;
    for (auto it = m_Bindings.constBegin(); it != m_Bindings.constEnd(); ++it) {
        values.insert(it.key(), evaluateSource(m_Joystick, it.value(), elementKind(it.key())));
    }

    QString activity;

    int numButtons = SDL_JoystickNumButtons(m_Joystick);
    for (int i = 0; i < numButtons && activity.isEmpty(); i++) {
        if (SDL_JoystickGetButton(m_Joystick, i) != 0) {
            activity = QCoreApplication::translate("SdlGamepadMapper", "Button %1").arg(i);
        }
    }

    int numHats = SDL_JoystickNumHats(m_Joystick);
    for (int i = 0; i < numHats && activity.isEmpty(); i++) {
        Uint8 hat = SDL_JoystickGetHat(m_Joystick, i);
        if (hat != SDL_HAT_CENTERED) {
            activity = QCoreApplication::translate("SdlGamepadMapper", "Hat %1 %2")
                    .arg(i).arg(describeHatDirection(hat));
        }
    }

    if (activity.isEmpty()) {
        int numAxes = qMin(SDL_JoystickNumAxes(m_Joystick), (int)m_AxisRestValues.size());
        int loudestAxis = -1;
        int loudestDeviation = 0;
        for (int i = 0; i < numAxes; i++) {
            int deviation = SDL_JoystickGetAxis(m_Joystick, i) - m_AxisRestValues[i];
            if (qAbs(deviation) < AXIS_ACTIVITY_THRESHOLD) {
                continue;
            }
            if (loudestAxis < 0 || qAbs(deviation) > qAbs(loudestDeviation)) {
                loudestAxis = i;
                loudestDeviation = deviation;
            }
        }

        if (loudestAxis >= 0) {
            activity = QCoreApplication::translate("SdlGamepadMapper", "Axis %1: %2")
                    .arg(loudestAxis)
                    .arg(SDL_JoystickGetAxis(m_Joystick, loudestAxis) / 32767.0, 0, 'f', 2);
        }
    }

    if (values == m_ElementValues && activity == m_RawActivity) {
        return;
    }

    m_ElementValues = values;
    m_RawActivity = activity;
    emit liveStateChanged();
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

    if (!m_TargetElement.isEmpty()) {
        QString binding;
        if (captureFromState(binding)) {
            QString element = m_TargetElement;
            m_Bindings[element] = binding;
            emit bindingsChanged();
            emit inputCaptured(element, binding);
            advanceQueue();
        }
    }

    refreshLiveState();
}

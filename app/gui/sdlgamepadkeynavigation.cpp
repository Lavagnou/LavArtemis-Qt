#include "sdlgamepadkeynavigation.h"

#include <QKeyEvent>
#include <QGuiApplication>
#include <QWindow>

#include "settings/mappingmanager.h"

#ifdef LAV_TEST_BUILD
#include "streaming/input/testinput.h"
#endif

// Every direction repeats as the prototype's gamepad and Android's DirRepeat (V8, audit of 2026-11-27): a first
// move at once, the second after 380 ms held, then one every 110 ms; letting go starts over
#define NAVIGATION_FIRST_REPEAT_DELAY 380
#define NAVIGATION_REPEAT_DELAY 110

// The stick counts past ~55 % of its travel (the prototype's 0.55; Android's system conversion takes 0.5). It was
// ±30000 (92 %): the stick had to be pushed to the end.
#define STICK_THRESHOLD 18000

// Polling: every 50 ms at rest (a new press), every 10 ms while a direction is held, so that 380 / 110 ms are not
// rounded up to 400 / 150
#define POLL_IDLE_MS 50
#define POLL_HELD_MS 10

#ifdef LAV_TEST_BUILD
namespace {
SdlGamepadKeyNavigation::KeySink s_TestKeySink;
}

void SdlGamepadKeyNavigation::setTestKeySink(KeySink sink)
{
    s_TestKeySink = std::move(sink);
}
#endif

SdlGamepadKeyNavigation::SdlGamepadKeyNavigation(StreamingPreferences* prefs)
    : m_Prefs(prefs),
      m_Enabled(false),
      m_UiNavMode(false),
      m_FirstPoll(false),
      m_HasFocus(false)
{
    m_PollingTimer = new QTimer(this);
    connect(m_PollingTimer, &QTimer::timeout, this, &SdlGamepadKeyNavigation::onPollingTimerFired);

    m_KeyRepeatTimer = new QTimer(this);
    connect(m_KeyRepeatTimer, &QTimer::timeout, this, &SdlGamepadKeyNavigation::onKeyRepeatTimerFired);
    // The window that had the key may lose the focus before its release comes: the repeat must not go on alone
    connect(qGuiApp, &QGuiApplication::focusWindowChanged, this, &SdlGamepadKeyNavigation::stopKeyRepeat);
    QCoreApplication::instance()->installEventFilter(this);
}

SdlGamepadKeyNavigation::~SdlGamepadKeyNavigation()
{
    QCoreApplication::instance()->removeEventFilter(this);
    disable();
}

bool SdlGamepadKeyNavigation::isOurPad(int deviceIndex)
{
    if (!MappingManager::isGameController(deviceIndex)) {
        return false;
    }
#ifdef LAV_TEST_BUILD
    // The user works on this PC while the bench runs: his own pads never drive the test client, only the channel's
    // virtual pad (as the stream, streaming/input/testinput.h)
    const char* name = SDL_GameControllerNameForIndex(deviceIndex);
    return name != nullptr && strcmp(name, kTestGamepadName) == 0;
#else
    return true;
#endif
}

void SdlGamepadKeyNavigation::enable()
{
    if (m_Enabled) {
        return;
    }

    // We have to initialize and uninitialize this in enable()/disable()
    // because we need to get out of the way of the Session class. If it
    // doesn't get to reinitialize the GC subsystem, it won't get initial
    // arrival events. Additionally, there's a race condition between
    // our QML objects being destroyed and SDL being deinitialized that
    // this solves too.
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) failed: %s",
                     SDL_GetError());
        return;
    }

    MappingManager mappingManager;
    mappingManager.applyMappings();

    // Drop all pending gamepad add events. SDL will generate these for us
    // on first init of the GC subsystem. We can't depend on them due to
    // overlapping lifetimes of SdlGamepadKeyNavigation instances, so we
    // will attach ourselves.
    //
    // NB: We use SDL_JoystickUpdate() instead of SDL_PumpEvents() because
    // the latter can do a bit more work that we want (like handling video
    // events that we intentionally do not want to process yet).
    SDL_JoystickUpdate();
    SDL_FlushEvent(SDL_CONTROLLERDEVICEADDED);

    // Open all currently attached game controllers
    int numJoysticks = SDL_NumJoysticks();
    for (int i = 0; i < numJoysticks; i++) {
        if (isOurPad(i)) {
            SDL_GameController* gc = SDL_GameControllerOpen(i);
            if (gc != nullptr) {
                m_Gamepads.append(gc);
            }
        }
    }

    m_Enabled = true;

    // Start the polling timer if the window is focused
    updateTimerState();
}

void SdlGamepadKeyNavigation::disable()
{
    if (!m_Enabled) {
        return;
    }

    m_Enabled = false;
    updateTimerState();
    Q_ASSERT(!m_PollingTimer->isActive());

    while (!m_Gamepads.isEmpty()) {
        SDL_GameControllerClose(m_Gamepads[0]);
        m_Gamepads.removeAt(0);
    }
    m_PadNav.clear();

    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}

void SdlGamepadKeyNavigation::notifyWindowFocus(bool hasFocus)
{
    m_HasFocus = hasFocus;
    updateTimerState();
}

void SdlGamepadKeyNavigation::sendMove(Direction direction)
{
    switch (direction) {
    case Up:
        if (m_UiNavMode) {
            // Back-tab
            sendKey(QEvent::Type::KeyPress, Qt::Key_Tab, Qt::ShiftModifier);
            sendKey(QEvent::Type::KeyRelease, Qt::Key_Tab, Qt::ShiftModifier);
        }
        else {
            sendKey(QEvent::Type::KeyPress, Qt::Key_Up);
            sendKey(QEvent::Type::KeyRelease, Qt::Key_Up);
        }
        break;
    case Down:
        if (m_UiNavMode) {
            sendKey(QEvent::Type::KeyPress, Qt::Key_Tab);
            sendKey(QEvent::Type::KeyRelease, Qt::Key_Tab);
        }
        else {
            sendKey(QEvent::Type::KeyPress, Qt::Key_Down);
            sendKey(QEvent::Type::KeyRelease, Qt::Key_Down);
        }
        break;
    case Left:
        sendKey(QEvent::Type::KeyPress, Qt::Key_Left);
        sendKey(QEvent::Type::KeyRelease, Qt::Key_Left);
        break;
    case Right:
        sendKey(QEvent::Type::KeyPress, Qt::Key_Right);
        sendKey(QEvent::Type::KeyRelease, Qt::Key_Right);
        break;
    case None:
        break;
    }
}

// A d-pad button: the press moves at once (the release follows when the button is let go), then the poll repeats it
// while it is held
void SdlGamepadKeyNavigation::pressDirection(SDL_JoystickID pad, Direction direction, bool down)
{
    const QEvent::Type type = down ? QEvent::Type::KeyPress : QEvent::Type::KeyRelease;
    switch (direction) {
    case Up:
        if (m_UiNavMode) sendKey(type, Qt::Key_Tab, Qt::ShiftModifier);
        else sendKey(type, Qt::Key_Up);
        break;
    case Down:
        if (m_UiNavMode) sendKey(type, Qt::Key_Tab);
        else sendKey(type, Qt::Key_Down);
        break;
    case Left:
        sendKey(type, Qt::Key_Left);
        break;
    case Right:
        sendKey(type, Qt::Key_Right);
        break;
    case None:
        return;
    }

    PadNav& nav = m_PadNav[pad];
    if (down) {
        nav.dpad = direction;
        nav.moves = 1;
        nav.lastMove = SDL_GetTicks();
    }
    else if (nav.dpad == direction) {
        nav.dpad = None;
        // The stick, if it is held, starts over from its first move
        nav.stick = None;
        nav.moves = 0;
    }
}

void SdlGamepadKeyNavigation::repeatHeldDirections()
{
    const Uint32 now = SDL_GetTicks();
    bool held = false;
    for (auto gc : std::as_const(m_Gamepads)) {
        const SDL_JoystickID id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gc));
        PadNav& nav = m_PadNav[id];

        // A d-pad release lost (flushed with the stale events of a stream): the button state decides
        if (nav.dpad != None) {
            static const SDL_GameControllerButton buttons[] = {SDL_CONTROLLER_BUTTON_INVALID, SDL_CONTROLLER_BUTTON_DPAD_UP,
                                                               SDL_CONTROLLER_BUTTON_DPAD_DOWN, SDL_CONTROLLER_BUTTON_DPAD_LEFT,
                                                               SDL_CONTROLLER_BUTTON_DPAD_RIGHT};
            if (!SDL_GameControllerGetButton(gc, buttons[nav.dpad])) {
                nav.dpad = None;
                nav.stick = None;
                nav.moves = 0;
            }
        }

        // The stick: vertical first in a diagonal (the PC's choice, kept: one move, never two)
        const short x = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX);
        const short y = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTY);
        const Direction stick = y < -STICK_THRESHOLD ? Up : y > STICK_THRESHOLD ? Down
                              : x < -STICK_THRESHOLD ? Left : x > STICK_THRESHOLD ? Right : None;

        Direction direction = nav.dpad;
        if (direction == None) {
            if (stick != nav.stick) {
                // A new stick direction (or back to rest): its first move goes at once
                nav.stick = stick;
                nav.moves = 0;
            }
            direction = stick;
        }
        if (direction == None) {
            nav.moves = 0;
            continue;
        }
        held = true;

        const Uint32 wait = nav.moves == 0 ? 0 : (nav.moves == 1 ? NAVIGATION_FIRST_REPEAT_DELAY : NAVIGATION_REPEAT_DELAY);
        if (nav.moves == 0 || now - nav.lastMove >= wait) {
            sendMove(direction);
            nav.lastMove = now;
            nav.moves++;
        }
    }

    // Poll tightly while a direction is held
    const int interval = held ? POLL_HELD_MS : POLL_IDLE_MS;
    if (m_PollingTimer->isActive() && m_PollingTimer->interval() != interval) {
        m_PollingTimer->setInterval(interval);
    }
}

void SdlGamepadKeyNavigation::onPollingTimerFired()
{
    SDL_Event event;

    // Update joystick state without pumping other events (see enable() comment)
    SDL_JoystickUpdate();

    // Discard any pending button events on the first poll to avoid picking up
    // stale input data from the stream session (like the quit combo).
    if (m_FirstPoll) {
        SDL_FlushEvent(SDL_CONTROLLERBUTTONDOWN);
        SDL_FlushEvent(SDL_CONTROLLERBUTTONUP);
        m_FirstPoll = false;
    }

    // Peep events rather than polling to avoid calling SDL_PumpEvents()
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT) == 1) {
        switch (event.type) {
        case SDL_QUIT:
            // SDL may send us a quit event since we initialize
            // the video subsystem on startup. If we get one,
            // forward it on for Qt to take care of.
            QCoreApplication::instance()->quit();
            break;
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
        {
            QEvent::Type type =
                    event.type == SDL_CONTROLLERBUTTONDOWN ?
                        QEvent::Type::KeyPress : QEvent::Type::KeyRelease;
            const bool down = event.type == SDL_CONTROLLERBUTTONDOWN;

            // Swap face buttons if needed
            if (m_Prefs->swapFaceButtons) {
                switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_A:
                    event.cbutton.button = SDL_CONTROLLER_BUTTON_B;
                    break;
                case SDL_CONTROLLER_BUTTON_B:
                    event.cbutton.button = SDL_CONTROLLER_BUTTON_A;
                    break;
                case SDL_CONTROLLER_BUTTON_X:
                    event.cbutton.button = SDL_CONTROLLER_BUTTON_Y;
                    break;
                case SDL_CONTROLLER_BUTTON_Y:
                    event.cbutton.button = SDL_CONTROLLER_BUTTON_X;
                    break;
                }
            }

            switch (event.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
                pressDirection(event.cbutton.which, Up, down);
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                pressDirection(event.cbutton.which, Down, down);
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                pressDirection(event.cbutton.which, Left, down);
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                pressDirection(event.cbutton.which, Right, down);
                break;
            case SDL_CONTROLLER_BUTTON_A:
                if (m_UiNavMode) {
                    sendKey(type, Qt::Key_Space);
                }
                else {
                    sendKey(type, Qt::Key_Return);
                }
                break;
            // The prototype's gamepad (design-port-spec §1.5), as on Android: B and Start go
            // back, Y opens the options of the focused item (the keyboard's Menu key), LB / RB
            // are the shoulder buttons (the keyboard's Q / E, PageUp / PageDown): previous /
            // next PC on the home, filter in the library, page in the settings. X does nothing.
            // A held 500 ms also opens the options: the pages time the Return key themselves.
            case SDL_CONTROLLER_BUTTON_B:
            case SDL_CONTROLLER_BUTTON_START:
                sendKey(type, Qt::Key_Escape);
                break;
            case SDL_CONTROLLER_BUTTON_Y:
                sendKey(type, Qt::Key_Menu);
                break;
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                sendKey(type, Qt::Key_PageUp);
                break;
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                sendKey(type, Qt::Key_PageDown);
                break;
            default:
                break;
            }
            break;
        }
        case SDL_CONTROLLERDEVICEADDED:
        {
            // A layout guessed earlier, now turned off: not a game controller any more
            if (MappingManager::isIgnoredGuess(event.cdevice.which) || !isOurPad(event.cdevice.which)) {
                break;
            }
            SDL_GameController* gc = SDL_GameControllerOpen(event.cdevice.which);
            if (gc != nullptr) {
                // SDL_CONTROLLERDEVICEADDED can be reported multiple times for the same
                // gamepad in rare cases, because SDL doesn't fixup the device index in
                // the SDL_CONTROLLERDEVICEADDED event if an unopened gamepad disappears
                // before we've processed the add event.
                if (!m_Gamepads.contains(gc)) {
                    m_Gamepads.append(gc);
                }
                else {
                    // We already have this game controller open
                    SDL_GameControllerClose(gc);
                }
            }
            break;
        }
        case SDL_CONTROLLERDEVICEREMOVED:
            // A pad unplugged: its repeat stops with it
            for (int i = 0; i < m_Gamepads.size(); i++) {
                if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(m_Gamepads[i])) == event.cdevice.which) {
                    SDL_GameControllerClose(m_Gamepads[i]);
                    m_Gamepads.removeAt(i);
                    break;
                }
            }
            m_PadNav.remove(event.cdevice.which);
            break;
        }
    }

    // Held d-pad directions and the stick, each pad on its own
    repeatHeldDirections();
}

void SdlGamepadKeyNavigation::sendKey(QEvent::Type type, Qt::Key key, Qt::KeyboardModifiers modifiers, bool fromPad)
{
    QGuiApplication* app = static_cast<QGuiApplication*>(QGuiApplication::instance());
    QWindow* focusWindow = app->focusWindow();
    m_Sending = true;
    bool sent = false;
    if (focusWindow != nullptr) {
        QKeyEvent keyPressEvent(type, key, modifiers);
        app->sendEvent(focusWindow, &keyPressEvent);
        sent = true;
    }
#ifdef LAV_TEST_BUILD
    else if (s_TestKeySink) {
        s_TestKeySink(type, key, modifiers);
        sent = true;
    }
#endif
    m_Sending = false;
    if (sent && fromPad) {
        emit gamepadInput();
    }
}

bool SdlGamepadKeyNavigation::eventFilter(QObject* watched, QEvent* event)
{
    Q_UNUSED(watched)
    if (m_Sending) {
        return false;
    }
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        const int key = keyEvent->key();
        if (key != Qt::Key_Up && key != Qt::Key_Down && key != Qt::Key_Left && key != Qt::Key_Right) {
            return false;
        }
        if (keyEvent->isAutoRepeat()) {
            // Windows' repeat (~500 ms then ~30 ms): ours replaces it, the gamepad's rhythm
            return true;
        }
        if (event->type() == QEvent::KeyPress) {
            // The same press is seen once per item it travels through (window, then the focused item and its
            // parents): only the first starts the repeat
            if (m_HeldKey != key) {
                m_HeldKey = key;
                m_HeldModifiers = keyEvent->modifiers();
                m_KeyRepeatTimer->start(NAVIGATION_FIRST_REPEAT_DELAY);
            }
        }
        else if (key == m_HeldKey) {
            stopKeyRepeat();
        }
    }
    else if (event->type() == QEvent::ApplicationDeactivate) {
        stopKeyRepeat();
    }
    return false;
}

void SdlGamepadKeyNavigation::onKeyRepeatTimerFired()
{
    if (m_HeldKey == 0) {
        m_KeyRepeatTimer->stop();
        return;
    }
    const Qt::Key key = static_cast<Qt::Key>(m_HeldKey);
    sendKey(QEvent::Type::KeyPress, key, m_HeldModifiers, false);
    sendKey(QEvent::Type::KeyRelease, key, m_HeldModifiers, false);
    if (m_KeyRepeatTimer->interval() != NAVIGATION_REPEAT_DELAY) {
        m_KeyRepeatTimer->setInterval(NAVIGATION_REPEAT_DELAY);
    }
}

void SdlGamepadKeyNavigation::stopKeyRepeat()
{
    m_HeldKey = 0;
    m_KeyRepeatTimer->stop();
}

void SdlGamepadKeyNavigation::updateTimerState()
{
    bool hasFocus = m_HasFocus;
#ifdef LAV_TEST_BUILD
    // No window of the test client ever gets the OS focus: the pad drives the UI whenever it is enabled (only the
    // channel's virtual pad is opened, isOurPad())
    hasFocus = true;
#endif
    if (m_PollingTimer->isActive() && (!hasFocus || !m_Enabled)) {
        m_PollingTimer->stop();
        m_PadNav.clear();
    }
    else if (!m_PollingTimer->isActive() && hasFocus && m_Enabled) {
        // Flush events on the first poll
        m_FirstPoll = true;

        // Poll every 50 ms for a new joystick event (10 ms while a direction is held)
        m_PollingTimer->start(POLL_IDLE_MS);
    }
}

void SdlGamepadKeyNavigation::setUiNavMode(bool uiNavMode)
{
    m_UiNavMode = uiNavMode;
}

int SdlGamepadKeyNavigation::getConnectedGamepads()
{
    Q_ASSERT(m_Enabled);

    int count = 0;
    int numJoysticks = SDL_NumJoysticks();
    for (int i = 0; i < numJoysticks; i++) {
        if (isOurPad(i)) {
            count++;
        }
    }

    return count;
}

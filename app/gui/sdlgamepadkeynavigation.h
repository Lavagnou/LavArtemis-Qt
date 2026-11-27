#pragma once

#include <QTimer>
#include <QEvent>
#include <QHash>

#ifdef LAV_TEST_BUILD
#include <functional>
#endif

#include "SDL_compat.h"

#include "settings/streamingpreferences.h"

// Drives the UI with a gamepad (keys sent to the focused window) and gives the arrow keys of the keyboard the
// same repeat (audit of 2026-11-27, E1 to E4 and V4): one move at once, the next after 380 ms held, then one every
// 110 ms -- the d-pad, the left stick (past ~55 % of its travel, vertical first in a diagonal) and the arrows of
// the keyboard (Windows' own repeat is ignored), as Android's DirRepeat and the prototype. Each pad keeps its own
// repeat: a pad at rest never restarts the one that is held.
class SdlGamepadKeyNavigation : public QObject
{
    Q_OBJECT

public:
    SdlGamepadKeyNavigation(StreamingPreferences* prefs);

    ~SdlGamepadKeyNavigation();

    Q_INVOKABLE void enable();

    Q_INVOKABLE void disable();

    Q_INVOKABLE void notifyWindowFocus(bool hasFocus);

    Q_INVOKABLE void setUiNavMode(bool settingsMode);

    Q_INVOKABLE int getConnectedGamepads();

#ifdef LAV_TEST_BUILD
    // Test build: no window ever has the OS focus (QGuiApplication::focusWindow() is null), so the keys the pad and
    // the key repeat produce go to this sink, which the test channel sets (the same delivery as its "key" command)
    using KeySink = std::function<void(QEvent::Type type, int key, Qt::KeyboardModifiers modifiers)>;
    static void setTestKeySink(KeySink sink);
#endif

signals:
    // A gamepad drove the UI (emitted after the key event it produced): the redesigned UI
    // (gui/v2) shows gamepad hints only then, never for a real keyboard.
    void gamepadInput();

protected:
    // The arrows of the keyboard: Windows' auto-repeat swallowed, ours instead
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    enum Direction { None = 0, Up, Down, Left, Right };

    // One pad's navigation state (V4: a pad at rest never resets the repeat of the one that is held)
    struct PadNav {
        Direction dpad = None;   // d-pad direction held (its button events), repeated while held
        Direction stick = None;  // direction the left stick is held in
        int moves = 0;           // moves sent since the held direction started
        Uint32 lastMove = 0;     // SDL_GetTicks() of the last one
    };

    void sendKey(QEvent::Type type, Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier, bool fromPad = true);

    void sendMove(Direction direction);

    void pressDirection(SDL_JoystickID pad, Direction direction, bool down);

    void repeatHeldDirections();

    void updateTimerState();

    void stopKeyRepeat();

    static bool isOurPad(int deviceIndex);

private slots:
    void onPollingTimerFired();

    void onKeyRepeatTimerFired();

private:
    StreamingPreferences* m_Prefs;
    QTimer* m_PollingTimer;
    QList<SDL_GameController*> m_Gamepads;
    QHash<SDL_JoystickID, PadNav> m_PadNav;
    bool m_Enabled;
    bool m_UiNavMode;
    bool m_FirstPoll;
    bool m_HasFocus;

    // Keyboard arrows: the key held (0 = none), its modifiers, how many repeats were sent
    QTimer* m_KeyRepeatTimer;
    int m_HeldKey = 0;
    Qt::KeyboardModifiers m_HeldModifiers;
    // Set while we send a key ourselves (pad, repeat): the event filter lets it through untouched
    bool m_Sending = false;
};

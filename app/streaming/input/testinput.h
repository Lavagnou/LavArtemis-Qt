#pragma once

// Test build only (LAV_TEST_BUILD): the stream accepts input from the testbench orchestrator
// and from nothing else. The user is working on this PC while tests run, so the real mouse,
// touch screen and game controllers must never reach the test host.
//
// - Mouse motion / button / wheel events are used only when SDL_MouseXxxEvent::which equals
//   kTestInjectedMouseId. The command channel (app/testctl) sets it on the events it pushes
//   with SDL_PushEvent. Real devices report other ids.
// - Game controllers: only a device named kTestGamepadName is opened and forwarded (the
//   command channel creates it as an SDL virtual joystick); physical pads are ignored.
// - Touch is NOT injectable in the test build, so every touch event is dropped: the filter
//   wants touchId == kTestInjectedMouseId, but handleTouchFingerEvent() also requires
//   SDL_GetTouchDeviceType(touchId) == SDL_TOUCH_DEVICE_DIRECT, which is INVALID for an id that
//   SDL never registered. The channel has no touch command.
// - The virtual pad is matched by NAME only: it must not borrow a known VID/PID (SDL would
//   apply that device's mapping and the name could change).
// - Keyboard needs no marker: the windows never get keyboard focus, so real keys do not arrive.

#include "SDL_compat.h"

constexpr Uint32 kTestInjectedMouseId = 0x4C415654; // 'LAVT'
constexpr const char* kTestGamepadName = "LavArtemis Test Pad";
// The channel's generic joystick ("joystick" command): a HID device SDL has no mapping for, to
// test the guessed layout. Never opened by the stream (only the pad is).
constexpr const char* kTestJoystickName = "LavArtemis Test Joystick";

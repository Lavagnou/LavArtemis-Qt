#pragma once

// Test build only (LAV_TEST_BUILD): the stream accepts input from the testbench orchestrator
// and from nothing else. The user is working on this PC while tests run, so the real mouse,
// touch screen and game controllers must never reach the test host.
//
// - Mouse motion / button / wheel events are used only when SDL_MouseXxxEvent::which equals
//   kTestInjectedMouseId (touch: SDL_TouchFingerEvent::touchId equals it). The orchestrator
//   sets it on events it pushes with SDL_PushEvent. Real devices report other ids.
// - Game controllers: only a device named kTestGamepadName is opened and forwarded (the
//   orchestrator creates it as an SDL virtual joystick); physical pads are ignored.
// - Keyboard needs no marker: the windows never get keyboard focus, so real keys do not arrive.

#include "SDL_compat.h"

constexpr Uint32 kTestInjectedMouseId = 0x4C415654; // 'LAVT'
constexpr const char* kTestGamepadName = "LavArtemis Test Pad";

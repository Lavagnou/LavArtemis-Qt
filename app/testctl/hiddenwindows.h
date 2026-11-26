#pragma once

// Test build only (CONFIG+=lav_test): the person using the PC never sees the testbench client's
// windows, and is never disturbed by them. Every top-level window of the GUI thread (the Qt
// window, the in-game menu, Qt Quick popups in a window of their own, the SDL stream window, any
// dialog) is:
// - cloaked (DWMWA_CLOAK) from its creation, before it is ever shown: DWM draws nothing of it
//   (no content, no frame, no shadow, not in Alt+Tab) and the mouse goes through it (a cloaked
//   window is not hit-tested), while for Windows, Qt and SDL it stays an ordinary shown window:
//   Qt keeps it exposed (it lays out, animates and renders), the stream keeps presenting, DWM
//   keeps composing it for PrintWindow(PW_RENDERFULLCONTENT);
// - at the bottom of the Z order, never topmost, never activated (WS_EX_NOACTIVATE, so no
//   taskbar button either; every activation refused), whatever Qt, SDL or the app ask (show,
//   raise(), requestActivate(), WindowStaysOnTopHint, SDL_SetWindowAlwaysOnTop).
//
// Not moved off the desktop: a top-level window is clipped by the desktop window, one past its
// edge gets an empty update region, Qt never sees it exposed and stops polishing it (layouts
// frozen half-way, popups stuck in their opening transition).
//
// LAV_TEST_VISIBLE=1 in the environment turns all of this off (debugging a scenario by eye):
// the windows then show where they would in the real app, still without taking the focus.

namespace TestWindows {

// Installs the window hooks on the calling thread (the GUI thread, before any window exists).
// Does nothing when LAV_TEST_VISIBLE=1 or outside Windows.
void install();

// True when the windows are hidden (install() ran and LAV_TEST_VISIBLE is not set).
bool active();

// A native window SDL created and set up (SDL replaces the window procedure of a window it did
// not create itself, so it is subclassed afterwards). Idempotent; call it after every
// (re)creation, before the window is shown. No-op when not active().
void adopt(void* hwnd);

}

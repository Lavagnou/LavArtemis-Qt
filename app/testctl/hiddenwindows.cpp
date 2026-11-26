#include "hiddenwindows.h"

#include <QtGlobal>
#include <QDebug>

#ifdef Q_OS_WIN32
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>

// How it works (all on the GUI thread, where Qt and SDL create every window):
// - a WH_CALLWNDPROCRET hook cloaks each new top-level window when its WM_CREATE returns, before
//   CreateWindowEx() can show it (WS_VISIBLE is applied after WM_CREATE);
// - a CBT hook creates each top-level window at the bottom of the Z order and refuses every
//   activation;
// - Qt's windows are subclassed at creation: WM_WINDOWPOSCHANGING is rewritten after Qt's own
//   handling (bottom of the Z order, no activation), WM_STYLECHANGING keeps WS_EX_NOACTIVATE;
// - SDL replaces the window procedure of a window it did not create itself (SetupWindowData),
//   and a subclass installed before that would call itself in a loop: an SDL window is subclassed
//   once SDL has set it up (its "SDL_WindowData" property, or Session calling adopt());
// - any other window of the thread (common dialog, IME) found topmost is put back at the bottom
//   right after the fact (the same WH_CALLWNDPROCRET hook).
namespace {

constexpr UINT_PTR kSubclassId = 0x4C415654; // "LAVT"

bool s_Active = false;
HHOOK s_CbtHook = nullptr;
HHOOK s_RetHook = nullptr;
bool s_Correcting = false;

bool isTopLevel(HWND hwnd)
{
    return !(GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_CHILD) && GetAncestor(hwnd, GA_PARENT) == GetDesktopWindow();
}

bool isQtWindow(HWND hwnd)
{
    wchar_t name[64] = {};
    return GetClassNameW(hwnd, name, 64) > 2 && name[0] == L'Q' && name[1] == L't';
}

void cloak(HWND hwnd)
{
    BOOL cloaked = FALSE;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked) {
        return;
    }
    const BOOL on = TRUE;
    const HRESULT hr = DwmSetWindowAttribute(hwnd, DWMWA_CLOAK, &on, sizeof(on));
    if (FAILED(hr)) {
        // The testbench's watch fails the scenario if this window ever shows
        qWarning("Test build: cannot cloak window %p (0x%08lx)", static_cast<void*>(hwnd), static_cast<unsigned long>(hr));
    }
}

LONG_PTR hiddenExStyle(LONG_PTR style)
{
    // Never activated, never on the taskbar, never topmost
    return (style | WS_EX_NOACTIVATE) & ~static_cast<LONG_PTR>(WS_EX_APPWINDOW | WS_EX_TOPMOST);
}

void applyExStyle(HWND hwnd)
{
    const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (hiddenExStyle(style) != style) {
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, hiddenExStyle(style));
    }
}

LRESULT CALLBACK subclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR)
{
    switch (msg) {
    case WM_WINDOWPOSCHANGING: {
        // After Qt's or SDL's own handling: the last word is ours. HWND_BOTTOM also takes a
        // topmost window out of the topmost band.
        const LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
        auto* pos = reinterpret_cast<WINDOWPOS*>(lParam);
        pos->flags |= SWP_NOACTIVATE;
        pos->flags &= ~SWP_NOZORDER;
        pos->hwndInsertAfter = HWND_BOTTOM;
        return result;
    }
    case WM_STYLECHANGING:
        if (wParam == static_cast<WPARAM>(GWL_EXSTYLE)) {
            const LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
            auto* style = reinterpret_cast<STYLESTRUCT*>(lParam);
            style->styleNew = static_cast<DWORD>(hiddenExStyle(style->styleNew));
            return result;
        }
        break;
    case WM_CREATE: {
        const LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
        if (result != -1) {
            applyExStyle(hwnd);
        }
        return result;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, subclassProc, kSubclassId);
        break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

bool isSubclassed(HWND hwnd)
{
    DWORD_PTR data = 0;
    return GetWindowSubclass(hwnd, subclassProc, kSubclassId, &data) != FALSE;
}

// Puts a window that is not subclassed back at the bottom, after the fact
void correctTopmost(HWND hwnd)
{
    if (s_Correcting || !(GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST)) {
        return;
    }
    s_Correcting = true;
    SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    s_Correcting = false;
}

LRESULT CALLBACK cbtProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HCBT_CREATEWND) {
        HWND hwnd = reinterpret_cast<HWND>(wParam);
        auto* create = reinterpret_cast<CBT_CREATEWNDW*>(lParam);
        if (!(create->lpcs->style & WS_CHILD) && create->lpcs->hwndParent != HWND_MESSAGE) {
            create->hwndInsertAfter = HWND_BOTTOM;
            if (isQtWindow(hwnd)) {
                SetWindowSubclass(hwnd, subclassProc, kSubclassId, 0);
            }
        }
    }
    else if (code == HCBT_ACTIVATE) {
        // Nothing of the test client is ever activated
        return 1;
    }
    return CallNextHookEx(s_CbtHook, code, wParam, lParam);
}

LRESULT CALLBACK retProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION) {
        const auto* msg = reinterpret_cast<const CWPRETSTRUCT*>(lParam);
        if (msg->message == WM_CREATE) {
            if (msg->lResult != -1 && isTopLevel(msg->hwnd)) {
                cloak(msg->hwnd);
            }
        }
        else if ((msg->message == WM_WINDOWPOSCHANGED || msg->message == WM_SHOWWINDOW) &&
                 isTopLevel(msg->hwnd) && !isSubclassed(msg->hwnd)) {
            cloak(msg->hwnd); // a window that escaped WM_CREATE (none expected): cloaked anyway
            if (GetPropW(msg->hwnd, L"SDL_WindowData") != nullptr) {
                TestWindows::adopt(msg->hwnd); // SDL has set the window up: safe to subclass
            }
            else {
                correctTopmost(msg->hwnd);
            }
        }
    }
    return CallNextHookEx(s_RetHook, code, wParam, lParam);
}

}

void TestWindows::install()
{
    if (s_Active) {
        return;
    }
    if (qgetenv("LAV_TEST_VISIBLE") == "1") {
        qInfo() << "Test build: LAV_TEST_VISIBLE=1, the windows show on the desktop";
        return;
    }
    const DWORD thread = GetCurrentThreadId();
    s_CbtHook = SetWindowsHookExW(WH_CBT, cbtProc, nullptr, thread);
    s_RetHook = SetWindowsHookExW(WH_CALLWNDPROCRET, retProc, nullptr, thread);
    if (s_CbtHook == nullptr || s_RetHook == nullptr) {
        // Never run the bench with windows on the user's desktop
        qFatal("Test build: cannot install the window hooks (error %lu)", GetLastError());
    }
    s_Active = true;
    qInfo() << "Test build: windows cloaked, at the bottom of the Z order, never activated (LAV_TEST_VISIBLE=1 shows them)";
}

bool TestWindows::active()
{
    return s_Active;
}

void TestWindows::adopt(void* window)
{
    HWND hwnd = static_cast<HWND>(window);
    if (!s_Active || hwnd == nullptr || !IsWindow(hwnd)) {
        return;
    }
    cloak(hwnd);
    if (isSubclassed(hwnd)) {
        return;
    }
    if (GetWindowThreadProcessId(hwnd, nullptr) != GetCurrentThreadId()) {
        qWarning() << "Test build: window" << window << "belongs to another thread, only cloaked";
        return;
    }
    if (!SetWindowSubclass(hwnd, subclassProc, kSubclassId, 0)) {
        qWarning() << "Test build: cannot subclass window" << window << GetLastError();
        return;
    }
    applyExStyle(hwnd);
    SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

#else

void TestWindows::install() {}
bool TestWindows::active() { return false; }
void TestWindows::adopt(void*) {}

#endif

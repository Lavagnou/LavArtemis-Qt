#pragma once

// Test build only (CONFIG+=lav_test): what the testbench command channel sees and does in
// the Qt Quick windows. No SDL and no session here, so tests/testguard can drive it on a
// small QML scene.
//
// Coordinates are window coordinates in logical (device-independent) pixels, those of
// QQuickItem::mapToScene; a screenshot is in device pixels (x devicePixelRatio).
// Input is sent to the window or item (QCoreApplication::sendEvent): no event loop runs inside
// a call (no QTest, whose helpers end with processEvents()), the real cursor never moves and
// no window is activated.

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QPointer>
#include <QRectF>
#include <QString>

class QObject;
class QQuickItem;
class QQuickWindow;

namespace UiProbe {

struct Item
{
    int window = 0;               // index in the window list passed to collect()
    QPointer<QQuickItem> item;
    QRectF rect;                  // the visible part: clipped by the window and clipping ancestors
    QString type;
    QString objectName;
    QString text;
    bool focus = false;           // on the focus chain: keyTarget() or a focused ancestor of it
    bool enabled = true;
    // A Réglages row (objectName "set:<key>", SettingsView2.qml): its label, and checked / value /
    // valueText when the row has them (SettingToggle, SettingChoice, SettingSlider)
    QJsonObject row;
    // A Flickable (ListView...): 1 while it moves (after a wheel or a flick it takes the next press
    // to stop itself, so a click then reaches nothing under it), 0 at rest; -1 = not a Flickable
    int moving = -1;
};

// Visible Qt Quick windows: `main` first (when visible), then the others in
// QGuiApplication::topLevelWindows() order.
QList<QQuickWindow*> windows(QQuickWindow* main);

// "Text" for QQuickText, "LavButton" for LavButton_QMLTYPE_12, "Button" for QQuickButton
QString typeName(const QObject* object);

// Every visible item (isVisible, effective opacity > 0, a non-empty visible part), depth
// first in child order. textOnly keeps only items with a text or an objectName.
QList<Item> collect(const QList<QQuickWindow*>& windows, bool textOnly = false);

QJsonObject toJson(const Item& item);

// Picks an item: {"text": exact} or {"objectName": exact}; "contains": true matches text as a
// substring; "index" picks the n-th match in tree order (default 0, negative counts from the
// end). False with an error when nothing matches.
bool find(const QList<Item>& items, const QJsonObject& spec, Item& out, QString& error);

// Whether a click at the centre of item.rect reaches the item: the topmost enabled item that
// takes that button at that point (paint order: z, then child order) must be the item, one of
// its ancestors or descendants, or there must be none. Takes clicks: an item with a pointer
// handler other than Hover/Wheel/Pinch (TapHandler cards...), a MouseArea, a button, a text
// field, a Flickable, a popup; anything accepting mouse buttons when it has no handler. The
// Controls overlay counts only while a modal popup is open. Returns the cover, else nullptr.
QQuickItem* coveringItem(const QList<QQuickWindow*>& windows, const Item& item,
                         Qt::MouseButton button = Qt::LeftButton);

// The window that takes `key`/`text`/`wheel` when none is named: the visible in-stream quick
// menu (a Qt::Tool window) if any, else the first window.
QQuickWindow* defaultInputWindow(const QList<QQuickWindow*>& windows);

// The timestamp synthetic events carry: on Windows the clock of real input (GetMessageTime(),
// i.e. GetTickCount() in ms), so that they never go back in time relative to the user's real
// mouse events reaching the same windows.
quint64 inputTimestamp();

// A click (press + release) at pos, delivered synchronously, without running the event loop.
void click(QQuickWindow* window, const QPointF& pos, Qt::MouseButton button);

// The item that takes `key` / `text`: the active focus item, or, in a window that never had
// the OS focus (Qt::WindowDoesNotAcceptFocus: no active focus at all, Qt Quick refuses to give
// one), the item at the end of its focus chain, the one that would get the active focus.
// Key events are sent to it, then to its parents until one accepts them, as Qt Quick does.
QQuickItem* keyTarget(QQuickWindow* window);

enum class KeyState { Click, Down, Up };
void key(QQuickWindow* window, int qtKey, Qt::KeyboardModifiers modifiers, KeyState state);

// Types text into keyTarget() (one key press/release per character, any Unicode text)
void typeText(QQuickWindow* window, const QString& text);

// angleDelta in eighths of a degree (120 = one notch); y > 0 scrolls up / away from the user
void wheel(QQuickWindow* window, const QPointF& pos, const QPoint& angleDelta);

// The type of the page on top of the first StackView in `window` ("HomeView"), empty if none.
// title (optional) gets its objectName, which the pages use as their title; busy (optional)
// whether the StackView is still animating a push/pop (the old page may still be on screen).
QString currentPage(QQuickWindow* window, QString* title = nullptr, bool* busy = nullptr);
// The item of that page (null without a StackView or a page)
QQuickItem* currentPageItem(QQuickWindow* window);

// Every focus ring on screen (a visible FocusRing item: v2/FocusRing.qml), its drawn box in scene
// coordinates (the ring's rectangles reach past the item; the item's scale counts), and what cuts it:
// the first ancestor that clips its children (clip: true, or layer.enabled: a layer renders its own
// box only) and does not hold the whole ring, or the window. {window, x, y, w, h, owner (type and
// objectName of the decorated item), cutBy: null or {type, objectName, x, y, w, h}}.
QJsonArray focusRings(const QList<QQuickWindow*>& windows);

}

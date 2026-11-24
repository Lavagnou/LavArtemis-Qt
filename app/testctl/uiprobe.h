#pragma once

// Test build only (CONFIG+=lav_test): what the testbench command channel sees and does in
// the Qt Quick windows. No SDL and no session here, so tests/testguard can drive it on a
// small QML scene.
//
// Coordinates are window coordinates in logical (device-independent) pixels, those of
// QQuickItem::mapToScene and QTest; a screenshot is in device pixels (x devicePixelRatio).
// Input is synthesized (QTest / QCoreApplication::sendEvent): the real cursor never moves and
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
    bool focus = false;           // active focus
    bool enabled = true;
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

// The window that takes `key`/`text`/`wheel` when none is named: the visible in-stream quick
// menu (a Qt::Tool window) if any, else the first window.
QQuickWindow* defaultInputWindow(const QList<QQuickWindow*>& windows);

// A click (press + release) at pos. QTest treats (0, 0) as "the centre of the window".
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

}

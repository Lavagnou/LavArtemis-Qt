#include "uiprobe.h"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QElapsedTimer>
#include <QMouseEvent>
#include <QSet>

#include <algorithm>

#ifdef Q_OS_WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif
#include <QWheelEvent>

namespace UiProbe {

QList<QQuickWindow*> windows(QQuickWindow* main)
{
    QList<QQuickWindow*> list;
    if (main != nullptr && main->isVisible()) {
        list.append(main);
    }
    const QWindowList topLevel = QGuiApplication::topLevelWindows();
    for (QWindow* window : topLevel) {
        auto* quickWindow = qobject_cast<QQuickWindow*>(window);
        if (quickWindow != nullptr && quickWindow != main && quickWindow->isVisible()) {
            list.append(quickWindow);
        }
    }
    return list;
}

QString typeName(const QObject* object)
{
    if (object == nullptr) {
        return {};
    }
    QString name = QString::fromLatin1(object->metaObject()->className());
    for (const char* suffix : {"_QMLTYPE_", "_QML_"}) {
        const int index = name.indexOf(QLatin1String(suffix));
        if (index > 0) {
            name.truncate(index);
        }
    }
    if (name.startsWith(QLatin1String("QQuick")) && name.size() > 6) {
        name = name.mid(6);
    }
    return name;
}

namespace {

void visit(QQuickItem* item, int windowIndex, qreal parentOpacity, const QRectF& clip, bool textOnly,
           const QSet<QQuickItem*>& focusChain, const QString& ancestorFid, QList<Item>& out)
{
    if (!item->isVisible()) {
        return;
    }
    const qreal opacity = parentOpacity * item->opacity();
    if (opacity <= 0) {
        return;
    }

    const QRectF sceneRect = item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
    const QRectF visible = sceneRect.intersected(clip);
    const QVariant fidValue = item->property("fid");
    const QString fid = fidValue.isValid() && fidValue.typeId() == QMetaType::QString ? fidValue.toString() : QString();
    if (!visible.isEmpty()) {
        Item entry;
        entry.window = windowIndex;
        entry.item = item;
        entry.rect = visible;
        entry.type = typeName(item);
        entry.objectName = item->objectName();
        const QVariant text = item->property("text");
        if (text.isValid() && text.canConvert<QString>()) {
            entry.text = text.toString();
        }
        entry.focus = focusChain.contains(item);
        entry.enabled = item->isEnabled();
        // A Flickable (ListView...) still moving after a wheel or a flick takes the next press to stop
        // itself: the item under it never sees that click
        const QVariant moving = item->property("moving");
        if (moving.isValid() && moving.typeId() == QMetaType::Bool) {
            entry.moving = moving.toBool() ? 1 : 0;
        }
        entry.fid = fid;
        entry.inFid = ancestorFid;
        const QVariant truncated = item->property("truncated");
        entry.truncated = truncated.isValid() && truncated.typeId() == QMetaType::Bool && truncated.toBool();
        // half a pixel: mapRectToScene of a scaled Stage is not exact
        entry.clipped = visible.width() < sceneRect.width() - 0.5 || visible.height() < sceneRect.height() - 0.5;
        if (entry.objectName.startsWith(QLatin1String("set:"))) {
            for (const char* name : {"label", "checked", "value", "valueText"}) {
                const QVariant value = item->property(name);
                if (!value.isValid()) {
                    continue;
                }
                const QJsonValue json = QJsonValue::fromVariant(value);
                if (json.isBool() || json.isDouble() || json.isString()) {
                    entry.row.insert(QLatin1String(name), json);
                }
            }
        }
        if (!textOnly || !entry.text.isEmpty() || !entry.objectName.isEmpty()) {
            out.append(entry);
        }
    }

    // Children may sit outside their parent; only a clipping parent hides them
    const QRectF childClip = item->clip() ? clip.intersected(sceneRect) : clip;
    const QList<QQuickItem*> children = item->childItems();
    for (QQuickItem* child : children) {
        visit(child, windowIndex, opacity, childClip, textOnly, focusChain, fid.isEmpty() ? ancestorFid : fid, out);
    }
}

}

QList<Item> collect(const QList<QQuickWindow*>& windows, bool textOnly)
{
    QList<Item> items;
    for (int i = 0; i < windows.size(); i++) {
        QQuickWindow* window = windows[i];
        if (window == nullptr || window->contentItem() == nullptr) {
            continue;
        }
        // Where keys go: keyTarget() and its ancestors that have the focus (focus scopes)
        QSet<QQuickItem*> focusChain;
        QQuickItem* target = keyTarget(window);
        for (QQuickItem* chain = target; chain != nullptr; chain = chain->parentItem()) {
            if (chain == target || chain->hasFocus()) {
                focusChain.insert(chain);
            }
        }
        visit(window->contentItem(), i, 1.0, QRectF(0, 0, window->width(), window->height()), textOnly,
              focusChain, QString(), items);
    }
    return items;
}

QJsonObject toJson(const Item& item)
{
    QJsonObject json;
    json["window"] = item.window;
    json["type"] = item.type;
    json["objectName"] = item.objectName;
    json["text"] = item.text;
    json["x"] = qRound(item.rect.x());
    json["y"] = qRound(item.rect.y());
    json["w"] = qRound(item.rect.width());
    json["h"] = qRound(item.rect.height());
    json["focus"] = item.focus;
    json["enabled"] = item.enabled;
    if (item.moving >= 0) {
        json["moving"] = item.moving == 1;
    }
    if (!item.row.isEmpty()) {
        json["row"] = item.row;
    }
    if (!item.fid.isEmpty()) {
        json["fid"] = item.fid;
    }
    if (!item.inFid.isEmpty()) {
        json["inFid"] = item.inFid;
    }
    if (item.truncated) {
        json["truncated"] = true;
    }
    if (item.clipped) {
        json["clipped"] = true;
    }
    return json;
}

bool find(const QList<Item>& items, const QJsonObject& spec, Item& out, QString& error)
{
    const QString text = spec.value("text").toString();
    const QString objectName = spec.value("objectName").toString();
    const bool contains = spec.value("contains").toBool(false);
    if (text.isEmpty() && objectName.isEmpty()) {
        error = QStringLiteral("give \"text\", \"objectName\" or \"x\"/\"y\"");
        return false;
    }

    QList<Item> matches;
    for (const Item& item : items) {
        if (!objectName.isEmpty() && item.objectName != objectName) {
            continue;
        }
        if (!text.isEmpty() && !(contains ? item.text.contains(text) : item.text == text)) {
            continue;
        }
        matches.append(item);
    }
    if (matches.isEmpty()) {
        error = !text.isEmpty() ? QStringLiteral("no visible item with text \"%1\"").arg(text)
                                : QStringLiteral("no visible item named \"%1\"").arg(objectName);
        return false;
    }

    int index = spec.value("index").toInt(0);
    if (index < 0) {
        index += matches.size();
    }
    if (index < 0 || index >= matches.size()) {
        error = QStringLiteral("index out of range: %1 match(es)").arg(matches.size());
        return false;
    }
    out = matches[index];
    return true;
}

namespace {

bool isRelated(QQuickItem* a, QQuickItem* b)
{
    for (QQuickItem* up = a; up != nullptr; up = up->parentItem()) {
        if (up == b) return true;
    }
    for (QQuickItem* up = b; up != nullptr; up = up->parentItem()) {
        if (up == a) return true;
    }
    return false;
}

bool isClickHandler(QObject* handler, Qt::MouseButton button)
{
    if (!handler->property("enabled").toBool()) {
        return false;
    }
    // Hovering, wheel and two-finger handlers never take a click
    if (handler->inherits("QQuickHoverHandler") || handler->inherits("QQuickWheelHandler") ||
        handler->inherits("QQuickPinchHandler")) {
        return false;
    }
    const QVariant buttons = handler->property("acceptedButtons");
    return !buttons.isValid() || qvariant_cast<Qt::MouseButtons>(buttons).testFlag(button);
}

// Whether `item` takes a click of `button`. Adding any pointer handler (even a HoverHandler)
// makes Qt Quick set acceptedMouseButtons to all buttons, so with handlers it is their kinds
// that decide, plus the types that take clicks by themselves.
bool takesClick(QQuickItem* item, Qt::MouseButton button)
{
    if (!item->isEnabled()) {
        return false;
    }
    bool hasHandlers = false;
    const QObjectList children = item->children();
    for (QObject* child : children) {
        if (child->inherits("QQuickPointerHandler")) {
            hasHandlers = true;
            if (isClickHandler(child, button)) {
                return true;
            }
        }
    }
    if (hasHandlers) {
        for (const char* type : {"QQuickMouseArea", "QQuickAbstractButton", "QQuickTextInput", "QQuickTextEdit",
                                 "QQuickFlickable", "QQuickPopupItem"}) {
            if (item->inherits(type)) {
                return item->acceptedMouseButtons().testFlag(button);
            }
        }
        return false;
    }
    return item->acceptedMouseButtons().testFlag(button);
}

bool modalPopupOpen(QQuickWindow* window)
{
    const QList<QObject*> objects = window->findChildren<QObject*>();
    for (QObject* object : objects) {
        if (object->inherits("QQuickPopup") && object->property("visible").toBool() &&
            object->property("modal").toBool()) {
            return true;
        }
    }
    return false;
}

// Topmost first: children above the item (z >= 0, last painted first), the item, then the
// children below it (z < 0). Stops at the target's family (reachable) or at an unrelated item
// that takes the click (covering). The Qt Quick Controls overlay (popups) covers everything
// under it only while a modal popup is open; its popups are ordinary items above the page.
bool hitTest(QQuickItem* item, const QPointF& scenePos, QQuickItem* target, Qt::MouseButton button,
             QQuickItem*& cover)
{
    if (!item->isVisible() || item->opacity() <= 0) {
        return false;
    }
    const QPointF local = item->mapFromScene(scenePos);
    const bool inside = QRectF(0, 0, item->width(), item->height()).contains(local);
    if (item->clip() && !inside) {
        return false;
    }

    QList<QQuickItem*> children = item->childItems();
    std::stable_sort(children.begin(), children.end(),
                     [](QQuickItem* a, QQuickItem* b) { return a->z() < b->z(); });

    if (item->inherits("QQuickOverlay")) {
        // Its children are the popups and their dimmers. With a modal popup open, anything
        // that stops the click there (dimmer included) is "the modal popup"; without one, only
        // the popups themselves can be in the way (a dimmer of a non-modal popup lets clicks by)
        const bool modal = item->window() != nullptr && modalPopupOpen(item->window());
        for (int i = children.size() - 1; i >= 0; i--) {
            if (!modal && !children[i]->inherits("QQuickPopupItem")) {
                continue;
            }
            if (hitTest(children[i], scenePos, target, button, cover)) {
                if (cover != nullptr && modal) {
                    cover = item;
                }
                return true;
            }
        }
        if (modal && inside) {
            cover = item;
            return true;
        }
        return false;
    }

    for (int i = children.size() - 1; i >= 0; i--) {
        if (children[i]->z() >= 0 && hitTest(children[i], scenePos, target, button, cover)) {
            return true;
        }
    }
    if (inside) {
        if (isRelated(item, target)) {
            cover = nullptr;
            return true;
        }
        if (takesClick(item, button)) {
            cover = item;
            return true;
        }
    }
    for (int i = children.size() - 1; i >= 0; i--) {
        if (children[i]->z() < 0 && hitTest(children[i], scenePos, target, button, cover)) {
            return true;
        }
    }
    return false;
}

}

QQuickItem* coveringItem(const QList<QQuickWindow*>& windows, const Item& item, Qt::MouseButton button)
{
    if (item.window < 0 || item.window >= windows.size() || item.item == nullptr) {
        return nullptr;
    }
    QQuickItem* cover = nullptr;
    hitTest(windows[item.window]->contentItem(), item.rect.center(), item.item, button, cover);
    return cover;
}

QQuickWindow* defaultInputWindow(const QList<QQuickWindow*>& windows)
{
    for (QQuickWindow* window : windows) {
        if (window->type() == Qt::Tool) {
            return window;
        }
    }
    return windows.isEmpty() ? nullptr : windows.first();
}

quint64 inputTimestamp()
{
#ifdef Q_OS_WIN32
    // The Windows platform plugin stamps real input with GetMessageTime(): the same clock
    return GetTickCount();
#else
    static QElapsedTimer clock;
    if (!clock.isValid()) {
        clock.start();
    }
    return static_cast<quint64>(clock.elapsed());
#endif
}

void click(QQuickWindow* window, const QPointF& pos, Qt::MouseButton button)
{
    // Sent straight to the window, like wheel(): no QTest (it ends with processEvents(), and a
    // nested event loop could start a whole stream inside the command), no OS cursor move
    const QPointF global = window->mapToGlobal(pos);
    const quint64 timestamp = inputTimestamp();
    QMouseEvent press(QEvent::MouseButtonPress, pos, pos, global, button, button, Qt::NoModifier);
    press.setTimestamp(timestamp);
    QCoreApplication::sendEvent(window, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, pos, pos, global, button, Qt::NoButton, Qt::NoModifier);
    release.setTimestamp(timestamp);  // a tap, not a press-and-hold
    QCoreApplication::sendEvent(window, &release);
}

QQuickItem* keyTarget(QQuickWindow* window)
{
    if (QQuickItem* active = window->activeFocusItem()) {
        return active;
    }
    // The window has no active focus (it never had the OS focus), but the focus chain is kept:
    // follow it from the root the way Qt Quick does when the window gets the focus
    QQuickItem* target = window->contentItem();
    while (target != nullptr && target->isFocusScope() && target->scopedFocusItem() != nullptr &&
           target->scopedFocusItem()->isEnabled()) {
        target = target->scopedFocusItem();
    }
    return target;
}

namespace {

// QQuickDeliveryAgent::deliverKeyEvent() without the active focus: the target first, then its
// parents until one accepts (Keys handlers, TextInput, StackView's Escape...)
void deliverKey(QQuickWindow* window, QEvent::Type type, int qtKey, Qt::KeyboardModifiers modifiers,
                const QString& text)
{
    QQuickItem* item = keyTarget(window);
    QKeyEvent event(type, qtKey, modifiers, text);
    event.setTimestamp(inputTimestamp());
    while (item != nullptr) {
        event.accept();
        QCoreApplication::sendEvent(item, &event);
        if (event.isAccepted()) {
            break;
        }
        item = item->parentItem();
    }
}

QString textForKey(int qtKey, Qt::KeyboardModifiers modifiers)
{
    if (modifiers & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) {
        return {};
    }
    if (qtKey >= 0x20 && qtKey < 0x7f) {
        const QChar c(qtKey);
        return (modifiers & Qt::ShiftModifier) ? QString(c) : QString(c.toLower());
    }
    switch (qtKey) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return QStringLiteral("\r");
    case Qt::Key_Tab:
        return QStringLiteral("\t");
    default:
        return {};
    }
}

}

void key(QQuickWindow* window, int qtKey, Qt::KeyboardModifiers modifiers, KeyState state)
{
    const QString text = textForKey(qtKey, modifiers);
    if (state != KeyState::Up) {
        deliverKey(window, QEvent::KeyPress, qtKey, modifiers, text);
    }
    if (state != KeyState::Down) {
        deliverKey(window, QEvent::KeyRelease, qtKey, modifiers, text);
    }
}

void typeText(QQuickWindow* window, const QString& text)
{
    for (int i = 0; i < text.size(); i++) {
        QString character(text[i]);
        if (text[i].isHighSurrogate() && i + 1 < text.size()) {
            character += text[++i];
        }
        const ushort u = character[0].unicode();
        if (u == '\n') {
            key(window, Qt::Key_Return, Qt::NoModifier, KeyState::Click);
            continue;
        }
        // Qt::Key of a printable ASCII character is its upper-case code
        const int qtKey = (u >= 0x20 && u < 0x7f) ? QChar(u).toUpper().unicode() : int(Qt::Key_unknown);
        deliverKey(window, QEvent::KeyPress, qtKey, Qt::NoModifier, character);
        deliverKey(window, QEvent::KeyRelease, qtKey, Qt::NoModifier, character);
    }
}

void wheel(QQuickWindow* window, const QPointF& pos, const QPoint& angleDelta)
{
    QWheelEvent event(pos, window->mapToGlobal(pos), QPoint(), angleDelta, Qt::NoButton, Qt::NoModifier,
                      Qt::NoScrollPhase, false);
    event.setTimestamp(inputTimestamp());
    QCoreApplication::sendEvent(window, &event);
}

namespace {

QQuickItem* findStackView(QQuickItem* item)
{
    // A style's StackView is a QML type (StackView_QMLTYPE_n) deriving from QQuickStackView
    if (item->inherits("QQuickStackView")) {
        return item;
    }
    const QList<QQuickItem*> children = item->childItems();
    for (QQuickItem* child : children) {
        if (QQuickItem* found = findStackView(child)) {
            return found;
        }
    }
    return nullptr;
}

}

QQuickItem* currentPageItem(QQuickWindow* window)
{
    if (window == nullptr || window->contentItem() == nullptr) {
        return nullptr;
    }
    QQuickItem* stack = findStackView(window->contentItem());
    return stack != nullptr ? qvariant_cast<QQuickItem*>(stack->property("currentItem")) : nullptr;
}

QString currentPage(QQuickWindow* window, QString* title, bool* busy)
{
    if (window == nullptr || window->contentItem() == nullptr) {
        return {};
    }
    QQuickItem* stack = findStackView(window->contentItem());
    if (stack == nullptr) {
        return {};
    }
    if (busy != nullptr) {
        *busy = stack->property("busy").toBool();
    }
    auto* page = qvariant_cast<QQuickItem*>(stack->property("currentItem"));
    if (page == nullptr) {
        return {};
    }
    if (title != nullptr) {
        *title = page->objectName();
    }
    return typeName(page);
}

namespace {

QJsonObject rectJson(const QRectF& r)
{
    return QJsonObject{{"x", qRound(r.x())}, {"y", qRound(r.y())}, {"w", qRound(r.width())}, {"h", qRound(r.height())}};
}

bool clipsChildren(QQuickItem* item)
{
    if (item->clip()) {
        return true;
    }
    // A layer renders the item's own box into a texture: whatever its children draw outside is lost
    QObject* layer = qvariant_cast<QObject*>(item->property("layer"));
    return layer != nullptr && layer->property("enabled").toBool();
}

bool holds(const QRectF& outer, const QRectF& inner)
{
    // half a pixel of slack for rounding
    return inner.left() >= outer.left() - 0.5 && inner.top() >= outer.top() - 0.5 &&
           inner.right() <= outer.right() + 0.5 && inner.bottom() <= outer.bottom() + 0.5;
}

void findRings(QQuickItem* item, QList<QQuickItem*>& out)
{
    if (!item->isVisible() || item->opacity() <= 0) {
        return;
    }
    if (typeName(item) == QLatin1String("FocusRing")) {
        out.append(item);
        return;
    }
    const QList<QQuickItem*> children = item->childItems();
    for (QQuickItem* child : children) {
        findRings(child, out);
    }
}

}

QJsonArray focusRings(const QList<QQuickWindow*>& windows)
{
    QJsonArray list;
    for (int w = 0; w < windows.size(); w++) {
        QQuickWindow* window = windows[w];
        QList<QQuickItem*> rings;
        findRings(window->contentItem(), rings);
        for (QQuickItem* ring : rings) {
            QRectF box;
            const QList<QQuickItem*> parts = ring->childItems();
            for (QQuickItem* part : parts) {
                if (part->isVisible() && part->opacity() > 0) {
                    box = box.united(part->mapRectToScene(QRectF(0, 0, part->width(), part->height())));
                }
            }
            if (box.isEmpty()) {
                continue;
            }
            QJsonObject entry = rectJson(box);
            entry["window"] = w;
            QQuickItem* owner = ring->parentItem();
            entry["owner"] = owner != nullptr ? typeName(owner) +
                    (owner->objectName().isEmpty() ? QString() : " (" + owner->objectName() + ")") : QString();
            QJsonValue cut = QJsonValue::Null;
            for (QQuickItem* up = owner; up != nullptr; up = up->parentItem()) {
                if (up == window->contentItem()) {
                    break;
                }
                if (!clipsChildren(up)) {
                    continue;
                }
                const QRectF bounds = up->mapRectToScene(QRectF(0, 0, up->width(), up->height()));
                if (!holds(bounds, box)) {
                    QJsonObject c = rectJson(bounds);
                    c["type"] = typeName(up);
                    c["objectName"] = up->objectName();
                    cut = c;
                    break;
                }
            }
            if (cut.isNull() && !holds(QRectF(0, 0, window->width(), window->height()), box)) {
                QJsonObject c = rectJson(QRectF(0, 0, window->width(), window->height()));
                c["type"] = QStringLiteral("window");
                c["objectName"] = QString();
                cut = c;
            }
            entry["cutBy"] = cut;
            list.append(entry);
        }
    }
    return list;
}

}

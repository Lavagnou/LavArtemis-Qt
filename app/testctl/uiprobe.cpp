#include "uiprobe.h"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QElapsedTimer>
#include <QMouseEvent>
#include <QSet>

#include <algorithm>
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
           const QSet<QQuickItem*>& focusChain, QList<Item>& out)
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
        if (!textOnly || !entry.text.isEmpty() || !entry.objectName.isEmpty()) {
            out.append(entry);
        }
    }

    // Children may sit outside their parent; only a clipping parent hides them
    const QRectF childClip = item->clip() ? clip.intersected(sceneRect) : clip;
    const QList<QQuickItem*> children = item->childItems();
    for (QQuickItem* child : children) {
        visit(child, windowIndex, opacity, childClip, textOnly, focusChain, out);
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
              focusChain, items);
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

// Topmost first: children above the item (z >= 0, last painted first), the item, then the
// children below it (z < 0). Stops at the target's family (reachable) or at an unrelated item
// that takes mouse buttons (covering).
bool hitTest(QQuickItem* item, const QPointF& scenePos, QQuickItem* target, QQuickItem*& cover)
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
    for (int i = children.size() - 1; i >= 0; i--) {
        if (children[i]->z() >= 0 && hitTest(children[i], scenePos, target, cover)) {
            return true;
        }
    }
    if (inside) {
        if (isRelated(item, target)) {
            cover = nullptr;
            return true;
        }
        if (item->isEnabled() && item->acceptedMouseButtons() != Qt::NoButton) {
            cover = item;
            return true;
        }
    }
    for (int i = children.size() - 1; i >= 0; i--) {
        if (children[i]->z() < 0 && hitTest(children[i], scenePos, target, cover)) {
            return true;
        }
    }
    return false;
}

}

QQuickItem* coveringItem(const QList<QQuickWindow*>& windows, const Item& item)
{
    if (item.window < 0 || item.window >= windows.size() || item.item == nullptr) {
        return nullptr;
    }
    QQuickItem* cover = nullptr;
    hitTest(windows[item.window]->contentItem(), item.rect.center(), item.item, cover);
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

void click(QQuickWindow* window, const QPointF& pos, Qt::MouseButton button)
{
    // Sent straight to the window, like wheel(): no QTest (it ends with processEvents(), and a
    // nested event loop could start a whole stream inside the command), no OS cursor move
    static QElapsedTimer clock;
    if (!clock.isValid()) {
        clock.start();
    }
    const QPointF global = window->mapToGlobal(pos);
    QMouseEvent press(QEvent::MouseButtonPress, pos, pos, global, button, button, Qt::NoModifier);
    press.setTimestamp(static_cast<quint64>(clock.elapsed()));
    QCoreApplication::sendEvent(window, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, pos, pos, global, button, Qt::NoButton, Qt::NoModifier);
    release.setTimestamp(static_cast<quint64>(clock.elapsed()) + 1);
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

}

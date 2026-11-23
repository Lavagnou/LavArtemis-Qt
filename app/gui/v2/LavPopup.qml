import QtQuick
import QtQuick.Controls

// Popup of the redesigned UI (dropdown lists, dialogs). Popups live in the window overlay, outside
// the page's scaled Stage: the content is laid out in design pixels and scaled by
// Theme.stageScale, so a popup keeps the page's proportions. Too tall for the window, it scrolls.
// Arrow keys move the focus between the popup's own controls (gamepad D-pad included).
Popup {
    id: popup

    property real k: Theme.stageScale
    property real designWidth: 560
    property real innerPadding: 28
    property real maxDesignHeight: Math.max(200, (Overlay.overlay ? Overlay.overlay.height : 800) / k - 60)
    readonly property real bodyWidth: designWidth - innerPadding * 2
    default property alias content: inner.data

    padding: 0
    focus: true
    margins: 12
    width: designWidth * k
    height: Math.min(inner.implicitHeight, maxDesignHeight) * k
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function isInside(item) {
        while (item) {
            if (item === inner) return true
            item = item.parent
        }
        return false
    }

    // Next focusable control of this popup, never one of the page behind it
    function moveFocus(forward) {
        var cur = inner.Window.activeFocusItem
        if (!cur || !isInside(cur)) return false
        var next = cur
        for (var n = 0; n < 200; n++) {
            next = next.nextItemInFocusChain(forward)
            if (!next || next === cur) return false
            if (isInside(next)) {
                next.forceActiveFocus(forward ? Qt.TabFocusReason : Qt.BacktabFocusReason)
                return true
            }
        }
        return false
    }

    background: Rectangle {
        radius: Theme.r * popup.k
        color: Theme.surface
        border.width: Math.max(1, Theme.border) * popup.k
        border.color: Theme.borderColor
    }

    contentItem: Flickable {
        id: flick
        clip: true
        contentWidth: width
        contentHeight: inner.implicitHeight * popup.k
        interactive: contentHeight > height + 1
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { visible: flick.interactive }

        Column {
            id: inner
            width: popup.designWidth
            padding: popup.innerPadding
            spacing: 18
            scale: popup.k
            transformOrigin: Item.TopLeft

            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_Down || event.key === Qt.Key_Right) {
                    event.accepted = popup.moveFocus(true)
                } else if (event.key === Qt.Key_Up || event.key === Qt.Key_Left) {
                    event.accepted = popup.moveFocus(false)
                }
            }
        }
    }
}

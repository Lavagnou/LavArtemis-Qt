import QtQuick

// The redesigned pages are laid out like the validated prototype: on a 1600-wide stage scaled up
// for wider windows, at real size and on the window's width between minLayoutWidth and 1600
// (pages must not assume 1600), scaled down only below minLayoutWidth. Children go into the
// stage; its height follows the window.
Item {
    id: stage

    property real designWidth: 1600
    // Narrower than the design, the page keeps its real size (buttons included) and lays itself
    // out on the window's width (prototype V5: shrinking the buttons was rejected). Only a window
    // narrower than minLayoutWidth is scaled down, and a wider one than the design is scaled up.
    property real minLayoutWidth: 1000
    readonly property real k: Math.max(0.5, Math.min(2.0, width >= designWidth ? width / designWidth
                                                                               : Math.min(1.0, width / minLayoutWidth)))
    default property alias content: inner.data

    clip: true

    onKChanged: if (visible) Theme.stageScale = k
    onVisibleChanged: if (visible) Theme.stageScale = k
    Component.onCompleted: if (visible) Theme.stageScale = k

    Item {
        id: inner
        width: stage.width / stage.k
        height: stage.height / stage.k
        scale: stage.k
        transformOrigin: Item.TopLeft
    }
}

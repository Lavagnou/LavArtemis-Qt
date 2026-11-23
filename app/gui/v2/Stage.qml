import QtQuick

// The redesigned pages are laid out like the validated prototype, on a 1600-wide design stage:
// - wider than 1600: the stage is scaled up;
// - narrower: it shrinks with the window, but never below minScale (shrinking all the way made
//   the buttons too small, real size made them too big), and the page then lays itself out on
//   the wider stage that leaves (pages must not assume 1600);
// - a page that needs more room (minLayoutWidth) is scaled further down instead.
// Children go into the stage; its height follows the window.
Item {
    id: stage

    property real designWidth: 1600
    property real minScale: 0.8
    property real minLayoutWidth: 1000
    readonly property real k: Math.max(0.5, Math.min(2.0, Math.max(width / designWidth, minScale),
                                                     width / minLayoutWidth))
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

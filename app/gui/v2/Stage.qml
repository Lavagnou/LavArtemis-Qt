import QtQuick

// The redesigned pages are laid out on a 1600-wide stage, like the validated prototype, and scaled
// to the window: the same composition on a 1080p laptop and a 1440p desktop, whatever the
// Windows scaling. Children go into the stage; its height follows the window.
Item {
    id: stage

    property real designWidth: 1600
    readonly property real k: Math.max(0.5, Math.min(2.0, width / designWidth))
    default property alias content: inner.data

    clip: true

    Item {
        id: inner
        width: stage.designWidth
        height: stage.height / stage.k
        scale: stage.k
        transformOrigin: Item.TopLeft
    }
}

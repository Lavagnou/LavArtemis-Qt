import QtQuick

// The pulsing "live" dot of the prototype (.lav-live): 8 px, current colour, lavpulse 1.4 s
// ease-in-out (opacity 1 -> .35, scale 1 -> .7).
Rectangle {
    id: dot
    property real k: 1.25
    width: 8 * k
    height: width
    radius: width / 2
    SequentialAnimation {
        running: dot.visible
        loops: Animation.Infinite
        ParallelAnimation {
            NumberAnimation { target: dot; property: "opacity"; from: 1; to: 0.35; duration: 700; easing.type: Easing.InOutQuad }
            NumberAnimation { target: dot; property: "scale"; from: 1; to: 0.7; duration: 700; easing.type: Easing.InOutQuad }
        }
        ParallelAnimation {
            NumberAnimation { target: dot; property: "opacity"; from: 0.35; to: 1; duration: 700; easing.type: Easing.InOutQuad }
            NumberAnimation { target: dot; property: "scale"; from: 0.7; to: 1; duration: 700; easing.type: Easing.InOutQuad }
        }
    }
}

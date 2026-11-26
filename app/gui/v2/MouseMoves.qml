import QtQuick

// When the mouse really moved last (Qt also sends hover events when an item moves or appears under
// a resting pointer: those must not steal the keyboard focus). Sets Theme.input to "mouse" on a
// real move. `recent` = moved within 200 ms.
HoverHandler {
    id: moves
    property real movedAt: 0
    property real touchAt: 0
    property point last: Qt.point(-1, -1)
    function recent() { return Date.now() - movedAt <= 200 && Date.now() - touchAt >= 900 }
    onPointChanged: {
        if (point.device.type !== PointerDevice.Mouse) return
        var p = point.scenePosition
        if (p.x === last.x && p.y === last.y) return
        last = Qt.point(p.x, p.y)
        movedAt = Date.now()
        if (Theme.input !== "mouse" && Date.now() - touchAt > 900) Theme.input = "mouse"
    }
}

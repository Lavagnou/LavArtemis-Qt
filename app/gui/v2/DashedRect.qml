import QtQuick
import QtQuick.Shapes

// A rounded rectangle with a dashed border (CSS `border: N px dashed`), filled or not.
Shape {
    id: dashed
    property real radius: 0
    property real borderWidth: 2
    property color borderColor: "black"
    property color fillColor: "transparent"
    preferredRendererType: Shape.CurveRenderer
    ShapePath {
        strokeWidth: dashed.borderWidth
        strokeColor: dashed.borderColor
        fillColor: dashed.fillColor
        strokeStyle: ShapePath.DashLine
        dashPattern: [3, 3]
        PathRectangle {
            x: dashed.borderWidth / 2
            y: dashed.borderWidth / 2
            width: dashed.width - dashed.borderWidth
            height: dashed.height - dashed.borderWidth
            radius: Math.max(0, dashed.radius - dashed.borderWidth / 2)
        }
    }
}

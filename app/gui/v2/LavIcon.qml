import QtQuick
import QtQuick.Shapes

// Line icons on a 24x24 grid (stroke drawn in `color`), so they follow the theme without image
// files. Shapes in the spirit of Lucide (ISC licence).
Item {
    id: icon

    property string name: "play"
    property color color: "white"
    property real stroke: 2
    property int size: 20
    // Any 24 x 24 SVG path (the prototype's own icons), instead of a named one
    property string path: ""
    property bool filled: false

    implicitWidth: size
    implicitHeight: size

    readonly property var paths: ({
        "play": "M7 5 L19 12 L7 19 Z",
        "chevron-left": "M15 6 L9 12 L15 18",
        "chevron-right": "M9 6 L15 12 L9 18",
        "chevron-down": "M6 9 L12 15 L18 9",
        "screens": "M3 5 H14 V13 H3 Z M10 9 H21 V17 H10 Z M7 21 H17",
        "monitor": "M3 4 H21 V16 H3 Z M8 21 H16 M12 16 V21",
        "sun": "M12 8 A4 4 0 1 1 11.99 8 M12 2 V4 M12 20 V22 M4.9 4.9 L6.3 6.3 M17.7 17.7 L19.1 19.1 M2 12 H4 M20 12 H22 M4.9 19.1 L6.3 17.7 M17.7 6.3 L19.1 4.9",
        "moon": "M20 14.5 A8 8 0 1 1 9.5 4 A6.5 6.5 0 0 0 20 14.5 Z",
        "gear": "M12 9 A3 3 0 1 1 11.99 9 M12 2 L13.4 4.6 L16.3 3.7 L16.6 6.7 L19.6 7 L18.7 9.9 L21.3 11.3 L18.7 12.7 L19.6 15.6 L16.6 15.9 L16.3 18.9 L13.4 18 L12 20.6 L10.6 18 L7.7 18.9 L7.4 15.9 L4.4 15.6 L5.3 12.7 L2.7 11.3 L5.3 9.9 L4.4 7 L7.4 6.7 L7.7 3.7 L10.6 4.6 Z",
        "grid": "M4 4 H10 V10 H4 Z M14 4 H20 V10 H14 Z M4 14 H10 V20 H4 Z M14 14 H20 V20 H14 Z",
        "dots": "M5 12 h0.01 M12 12 h0.01 M19 12 h0.01",
        "stop": "M6 6 H18 V18 H6 Z",
        "refresh": "M20 12 A8 8 0 1 1 17.7 6.3 M20 4 V9 H15",
        "link": "M10 14 L14 10 M8.5 11.5 L6 14 A3.5 3.5 0 0 0 11 19 L13.5 16.5 M15.5 12.5 L18 10 A3.5 3.5 0 0 0 13 5 L10.5 7.5",
        "eye-off": "M3 3 L21 21 M10.6 6.1 A10 10 0 0 1 22 12 A16 16 0 0 1 19 15.5 M6.6 6.6 A16 16 0 0 0 2 12 S5.5 19 12 19 A9.7 9.7 0 0 0 17.4 17.4",
        "gauge": "M4 18 A8 8 0 1 1 20 18 M12 14 L16 9",
        "pencil": "M4 20 L8 19 L19 8 L16 5 L5 16 Z M14 7 L17 10",
        "trash": "M4 7 H20 M9 7 V4 H15 V7 M6 7 L7 20 H17 L18 7",
        "eye": "M2 12 Q12 3 22 12 Q12 21 2 12 Z M12 9.5 A2.5 2.5 0 1 1 11.99 9.5",
        "search": "M11 4 A7 7 0 1 1 10.99 4 M16 16 L21 21",
        "power": "M12 3 V12 M7 6.3 A8 8 0 1 0 17 6.3",
        "plus": "M12 5 V19 M5 12 H19",
        "lock": "M6 11 H18 V21 H6 Z M8.5 11 V7.5 A3.5 3.5 0 0 1 15.5 7.5 V11",
        "back": "M19 12 H5 M11 6 L5 12 L11 18",
        "check": "M5 12.5 L10 17 L19 7"
    })

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        scale: icon.width / 24
        transformOrigin: Item.TopLeft
        width: 24
        height: 24

        ShapePath {
            strokeColor: icon.color
            strokeWidth: icon.name === "dots" ? 3.2 : icon.stroke
            fillColor: (icon.path === "" && icon.name === "play") || icon.filled ? icon.color : "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: icon.path !== "" ? icon.path : (icon.paths[icon.name] || "") }
        }
    }
}

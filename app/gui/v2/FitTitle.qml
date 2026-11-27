import QtQuick

// A title that gets smaller (down to minSize) to stay on one line, then takes more lines (maximumLineCount, elided
// only past them): the Scène's title and Braise's focused game on the home (scenario 21 of the testbench).
// Not `fontSizeMode: Text.HorizontalFit`: with a wrapMode, Qt keeps the height of the unscaled text (two lines of
// the big size) while drawing one small line, and a page laid out on the title's height left a hole under it.
// The size is the one whose single line, measured at maxSize by `probe`, fits the width. Family and weight are set
// here, not through `font`: a binding on font.* that reads `font` loops (font.letterSpacing from font.pixelSize).
Text {
    id: title
    property real maxSize: 42
    property real minSize: 26
    property real spacingEm: 0          // font.letterSpacing, in em
    property string family: ""
    property int weight: Font.Normal
    // the width of the text on one line at maxSize (a title that sizes itself on its text: Ludique's capsule)
    readonly property real naturalWidth: probe.implicitWidth
    // 2 % under the proportional size: the advance of a glyph does not scale exactly with the font
    readonly property real fitSize: probe.implicitWidth <= width || width <= 0 ? maxSize
        : Math.max(minSize, Math.floor(maxSize * width / probe.implicitWidth * 0.98 * 10) / 10)

    wrapMode: Text.WordWrap
    maximumLineCount: 2
    elide: Text.ElideRight
    font.family: family
    font.weight: weight
    font.pixelSize: fitSize
    font.letterSpacing: spacingEm * fitSize

    Text {
        id: probe
        visible: false
        text: title.text
        font.family: title.family
        font.weight: title.weight
        font.pixelSize: title.maxSize
        font.letterSpacing: title.spacingEm * title.maxSize
    }
}

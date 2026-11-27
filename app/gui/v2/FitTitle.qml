import QtQuick

// A title that gets smaller (down to minSize) to stay on one line, then takes more lines (maximumLineCount, elided
// only past them): the Scène's title and Braise's focused game on the home (scenario 21 of the testbench).
// Not `fontSizeMode: Text.HorizontalFit`: with a wrapMode, Qt keeps the height of the unscaled text (two lines of
// the big size) while drawing one small line, and a page laid out on the title's height left a hole under it.
// The size is the one whose single line, measured at maxSize by `probe`, fits the width.
Text {
    id: title
    property real maxSize: 42
    property real minSize: 26
    property real spacingEm: 0          // font.letterSpacing, in em
    // the width of the text on one line at maxSize (a title that sizes itself on its text: Ludique's capsule)
    readonly property real naturalWidth: probe.implicitWidth

    wrapMode: Text.WordWrap
    maximumLineCount: 2
    elide: Text.ElideRight
    // 2 % under the proportional size: the advance of a glyph does not scale exactly with the font
    font.pixelSize: probe.implicitWidth <= width || width <= 0 ? maxSize
        : Math.max(minSize, Math.floor(maxSize * width / probe.implicitWidth * 0.98 * 10) / 10)
    font.letterSpacing: spacingEm * font.pixelSize

    Text {
        id: probe
        visible: false
        text: title.text
        font.family: title.font.family
        font.weight: title.font.weight
        font.pixelSize: title.maxSize
        font.letterSpacing: title.spacingEm * title.maxSize
    }
}

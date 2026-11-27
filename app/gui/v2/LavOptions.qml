import QtQuick
import QtQuick.Controls
import QtQuick.Effects

// Options and dialogs of the prototype (design-port-spec §1.8), one shape for every kind: a head
// line (kind · where), a title, an optional cover, a note (warning box or muted text), then the
// entries. Two looks:
//  - centred dialog (gamepad, keyboard, touch, long press, and every dialog that is not a
//    right-click menu): per theme, Braise 520 r26, Affiche 580 square with a 3 px border and a list
//    under a 3 px rule, Ludique 560 r40; the main entry first, "Annuler" last;
//  - popover at the pointer (right click or "⋯" with the mouse, PC): 300 wide, kind in small
//    capitals, the closing warning as "⚠ Ferme …", no "Annuler", no icons on the other rows.
// An entry with an `icon` (an SVG path: « Plus » of the in-game menu, menu workshop 2026-11-26) shows it
// before its label, from the start of the row, as the tiles do: the accent's stroke in Braise, a round
// chip in Ludique, none in Affiche (its menu has no icons) nor in the popover.
// Sizes are prototype px x k (k = 1.25 x Theme.stageScale: the overlay is not inside a Stage).
Popup {
    id: opts

    // Set by open...(): {title, head, cover, warn, info, note, popNote, entries: [{l, a, main, play, icon}]}
    property string title: ""
    property string head: ""
    property url cover: ""
    property bool warn: false
    property bool info: false
    property string note: ""
    property string popNote: ""
    property var entries: []
    property bool popover: false
    property real atX: 0
    property real atY: 0
    // Extra content under the note (a text field, a PIN): an Item, owned by the caller
    property Item extra: null
    property int focusIndex: 0

    signal picked(string action)

    // The page under the dialog, which gets the keys back as soon as the dialog starts closing. Qt takes the
    // keys from a popup at the START of its exit animation (Material: 220 ms) and gives them to nobody: waiting
    // for onClosed, at its end, lost a key pressed in between (Échap or B, then a direction or A at once).
    // onClosed gives them again, after Qt's own focus restore.
    property Item returnFocusTo: null
    onAboutToHide: if (returnFocusTo) returnFocusTo.forceActiveFocus()
    onClosed: if (returnFocusTo) returnFocusTo.forceActiveFocus()

    // The extra item is the caller's, hidden until a dialog shows it; one at a time. An extra with a
    // `focusTarget` (a text field) gets the keys when the dialog opens
    property Item shownExtra: null
    onExtraChanged: {
        if (shownExtra && shownExtra !== extra) shownExtra.visible = false
        shownExtra = extra
        if (extra) {
            extra.parent = extraHolder
            extra.width = Qt.binding(function() { return extraHolder.width })
            extra.visible = true
        }
    }
    onOpened: if (extra && extra.focusTarget) extra.focusTarget.forceActiveFocus()
    // Named for the test channel (testbench helpers.popup_texts): the popup's own item
    Component.onCompleted: if (background && background.parent) background.parent.objectName = "LavOptions"

    readonly property real k: 1.25 * Theme.stageScale
    readonly property var shown: popover ? entries.filter(function(e) { return e.a !== "cancel" }) : entries
    readonly property int cols: popover ? 1 : (shown.length > 14 ? 3 : (shown.length > 8 ? 2 : 1))

    function showDialog(cfg) { apply(cfg); popover = false; open() }
    function showPopover(cfg, x, y) { apply(cfg); popover = true; atX = x; atY = y; open() }
    function apply(cfg) {
        title = cfg.title || ""
        head = cfg.head || ""
        cover = cfg.cover || ""
        warn = !!cfg.warn
        info = !!cfg.info
        note = cfg.note || ""
        popNote = cfg.popNote || ""
        entries = cfg.entries || []
        extra = cfg.extra || null
        focusIndex = cfg.focusIndex || 0
    }
    function pick(i) {
        var e = shown[i]
        if (!e) return
        close()
        picked(e.a)
    }
    function move(dx, dy) {
        var n = shown.length
        if (n === 0) return
        if (cols === 1) { focusIndex = Math.max(0, Math.min(n - 1, focusIndex + (dy !== 0 ? dy : dx))); return }
        var r = Math.floor(focusIndex / cols), c = focusIndex % cols
        if (dy !== 0) r += dy
        else c = Math.max(0, Math.min(cols - 1, c + dx))
        focusIndex = Math.max(0, Math.min(n - 1, r * cols + c))
    }

    parent: Overlay.overlay
    modal: true
    focus: true
    dim: !popover
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside | Popup.CloseOnPressOutsideParent
    padding: 0

    readonly property real boxW: popover ? 300 * k
                                         : ((Theme.affiche ? 580 : (Theme.ludique ? 560 : 520)) + (cols - 1) * 250) * k
    // The area it is centred in: the overlay. The in-game menu says its own size: on the `offscreen` platform
    // (captures) the overlay of its window stays 0 x 0, and the dialog came out centred on the window's corner
    property real areaWidth: parent ? parent.width : 0
    property real areaHeight: parent ? parent.height : 0
    width: boxW
    x: popover ? Math.max(8 * k, (atX + 4 * k + 310 * k > areaWidth - 8 * k) ? atX - 314 * k : atX + 4 * k)
               : (areaWidth - width) / 2
    y: popover ? Math.max(8 * k, (atY + 4 * k + height > areaHeight - 8 * k) ? atY - height - 4 * k : atY + 4 * k)
               : (areaHeight - height) / 2

    Overlay.modal: Item {
        // Backdrop of the dialog: a light veil and a 3 px blur of the page (prototype backdrop-filter)
        Rectangle {
            anchors.fill: parent
            visible: !opts.popover
            color: Theme.braise ? (Theme.dark ? Qt.rgba(8/255, 7/255, 6/255, 0.38) : Qt.rgba(60/255, 45/255, 30/255, 0.22))
                 : Theme.affiche ? (Theme.dark ? Qt.rgba(0, 0, 0, 0.55) : Qt.rgba(17/255, 17/255, 17/255, 0.4))
                 : (Theme.dark ? Qt.rgba(0, 0, 0, 0.35) : Qt.rgba(27/255, 21/255, 48/255, 0.35))
        }
    }

    background: Item {
        RectangularShadow {
            anchors.fill: bg
            visible: !Theme.affiche && !(Theme.ludique && Theme.dark && !opts.popover)
            offset.y: 24 * opts.k
            blur: (opts.popover ? 54 : 60) * opts.k
            radius: bg.radius
            color: opts.popover ? Qt.rgba(0, 0, 0, 0.55)
                 : (Theme.ludique ? Qt.rgba(27/255, 21/255, 48/255, 0.35) : Qt.rgba(0, 0, 0, 0.6))
        }
        Rectangle {
            id: bg
            anchors.fill: parent
            radius: opts.popover ? Theme.popR * opts.k : (Theme.affiche ? 0 : (Theme.ludique ? 40 : 26) * opts.k)
            color: opts.popover ? Theme.surface
                 : (Theme.braise ? (Theme.dark ? "#1A1816" : "#FFFFFF") : Theme.affiche ? (Theme.dark ? "#141414" : "#ECECE8")
                    : (Theme.dark ? "#221C3D" : "#FFFFFF"))
            border.width: opts.popover ? Theme.border * opts.k : (Theme.affiche ? 3 * opts.k : 0)
            border.color: Theme.affiche ? Theme.fg : Theme.line
        }
    }

    contentItem: Column {
        id: content
        // The dialog's keys. On the content, which takes the popup's focus: a Popup is not an Item, and
        // `Keys` attached to it was dropped (« Could not attach Keys property »), the arrows and A did nothing.
        // Échap is not taken: it goes on to the popup (CloseOnEscape)
        focus: true
        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Up) { Theme.input = Theme.input === "pad" ? "pad" : "keys"; opts.move(0, -1); event.accepted = true }
            else if (event.key === Qt.Key_Down) { Theme.input = Theme.input === "pad" ? "pad" : "keys"; opts.move(0, 1); event.accepted = true }
            else if (event.key === Qt.Key_Left) { opts.move(-1, 0); event.accepted = true }
            else if (event.key === Qt.Key_Right) { opts.move(1, 0); event.accepted = true }
            else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
                if (!event.isAutoRepeat) opts.pick(opts.focusIndex)
                event.accepted = true
            } else if (event.key === Qt.Key_Menu) { opts.close(); event.accepted = true }
        }
        padding: opts.popover ? 10 * opts.k : (Theme.affiche ? 26 * opts.k : (Theme.ludique ? 28 : 24) * opts.k)
        leftPadding: opts.popover ? 10 * opts.k : (Theme.affiche ? 28 : (Theme.ludique ? 28 : 24)) * opts.k
        rightPadding: leftPadding
        spacing: opts.popover ? 3 * opts.k : (Theme.affiche ? 14 : 12) * opts.k
        readonly property real innerW: opts.width - leftPadding - rightPadding

        // ---- head: cover, kind line, title (laid out by hand: anchors inside a Row are ignored)
        Item {
            id: headBox
            width: content.innerW
            readonly property real topPad: opts.popover ? 2 * opts.k : 0
            readonly property real bottomPad: opts.popover ? 8 * opts.k : (Theme.affiche ? 0 : 4 * opts.k)
            readonly property real gap: (opts.popover ? 10 : (Theme.braise ? 18 : 20)) * opts.k
            readonly property real coverW: headCover.visible ? headCover.width + gap : 0
            readonly property real bodyH: Math.max(headCover.visible ? headCover.height : 0, headText.height)
            height: topPad + bodyH + bottomPad
            Image {
                id: headCover
                visible: opts.cover.toString() !== "" && status !== Image.Error
                x: opts.popover ? 4 * opts.k : 0
                width: (opts.popover ? 32 : (Theme.braise ? 72 : 80)) * opts.k
                height: (opts.popover ? 48 : (Theme.braise ? 108 : 120)) * opts.k
                y: headBox.topPad + (headBox.bodyH - height) / 2
                source: opts.cover
                fillMode: Image.PreserveAspectCrop
                layer.enabled: !Theme.affiche && Theme.effects
                layer.effect: MultiEffect { maskEnabled: true; maskSource: coverMask }
            }
            Rectangle {
                id: coverMask
                visible: false
                layer.enabled: true
                width: headCover.width; height: headCover.height
                radius: (opts.popover ? Theme.thumbR : (Theme.braise ? 10 : 18)) * opts.k
            }
            Column {
                id: headText
                x: headBox.coverW + (opts.popover ? 4 * opts.k : 0)
                // Affiche: the text sits on the cover's bottom; the others: centred on it
                y: headBox.topPad + (Theme.affiche && !opts.popover ? headBox.bodyH - height : (headBox.bodyH - height) / 2)
                width: headBox.width - x
                spacing: (opts.popover ? 2 : (Theme.affiche ? 8 : (Theme.ludique ? 4 : 6))) * opts.k
                Text {
                    width: parent.width
                    visible: text !== ""
                    text: opts.popover ? opts.head.split(Theme.affiche ? " — " : " · ")[0].toUpperCase() : opts.head
                    color: opts.popover ? Theme.muted : (Theme.braise ? Theme.accentText : (Theme.affiche ? Theme.fg : Theme.muted))
                    elide: Text.ElideRight
                    font.family: Theme.textFont
                    font.pixelSize: (opts.popover ? 11 : (Theme.braise ? 13 : 14)) * opts.k
                    font.weight: Theme.affiche && !opts.popover ? Font.ExtraBold : Font.Bold
                    font.letterSpacing: opts.popover ? 0.04 * 11 * opts.k : 0
                }
                Text {
                    width: parent.width
                    text: opts.title
                    color: Theme.fg
                    wrapMode: Text.WordWrap
                    font.family: Theme.displayFont
                    font.weight: Theme.displayWeight
                    readonly property real px: opts.popover ? 16 : (Theme.affiche ? 44 : (Theme.ludique ? 34 : 28))
                    font.pixelSize: px * opts.k
                    font.letterSpacing: (opts.popover ? Theme.tracking : (Theme.affiche ? -0.045 : Theme.tracking)) * px * opts.k
                    lineHeight: opts.popover ? 1.15 : (Theme.affiche ? 0.92 : (Theme.ludique ? 1.02 : 1.1))
                }
            }
        }

        // ---- popover note: "⚠ Ferme …"
        Rectangle {
            visible: opts.popover && opts.popNote !== ""
            x: 2 * opts.k
            width: content.innerW - 4 * opts.k
            height: popNoteText.implicitHeight + 14 * opts.k
            radius: Theme.rs * opts.k
            color: Theme.warnBg
            Text {
                id: popNoteText
                x: 10 * opts.k
                width: parent.width - 20 * opts.k
                anchors.verticalCenter: parent.verticalCenter
                text: "⚠  " + opts.popNote
                color: Theme.fg
                wrapMode: Text.WordWrap
                font.family: Theme.textFont
                font.pixelSize: 12.5 * opts.k
                font.weight: Font.DemiBold
            }
        }
        Item { visible: opts.popover && opts.popNote !== ""; width: 1; height: 3 * opts.k }

        // ---- dialog note: warning box, or muted text
        Rectangle {
            visible: !opts.popover && opts.warn && opts.note !== ""
            width: content.innerW
            height: warnText.implicitHeight + (Theme.ludique ? 28 : 24) * opts.k
            radius: (Theme.affiche ? 0 : (Theme.ludique ? 22 : 14)) * opts.k
            color: Theme.braise ? (Theme.dark ? "#2A1A10" : "#FBE3D4") : Theme.warnBg
            border.width: Theme.braise ? opts.k : 0
            border.color: Theme.braise ? (Theme.dark ? "#FF6A2B" : "#E8541A") : "transparent"
            Text {
                id: warnText
                x: (Theme.ludique ? 18 : 14) * opts.k
                width: parent.width - 2 * x
                anchors.verticalCenter: parent.verticalCenter
                text: opts.note
                wrapMode: Text.WordWrap
                color: Theme.braise ? (Theme.dark ? "#F3D9C9" : "#5A2A12") : Theme.fg
                lineHeight: Theme.braise ? 1.5 : 1.45
                font.family: Theme.textFont
                font.pixelSize: (Theme.braise ? 14 : 15) * opts.k
                font.weight: Theme.braise ? Font.Normal : Font.DemiBold
            }
        }
        Text {
            visible: !opts.popover && !opts.warn && opts.info && opts.note !== ""
            width: content.innerW
            text: opts.note
            wrapMode: Text.WordWrap
            color: Theme.muted
            lineHeight: 1.5
            font.family: Theme.textFont
            font.pixelSize: 15 * opts.k
        }

        Item {
            id: extraHolder
            visible: opts.extra !== null
            width: content.innerW
            height: opts.extra ? opts.extra.implicitHeight : 0
        }

        // ---- entries
        Rectangle {
            // Affiche: the list starts under a 3 px rule
            visible: Theme.affiche && !opts.popover
            width: content.innerW
            height: 3 * opts.k
            color: Theme.fg
        }
        Grid {
            id: grid
            columns: opts.cols
            width: content.innerW
            columnSpacing: (Theme.affiche && !opts.popover ? 18 : 12) * opts.k
            rowSpacing: opts.popover ? 3 * opts.k : (Theme.affiche ? 0 : 12 * opts.k)
            readonly property real cellW: (width - (opts.cols - 1) * columnSpacing) / opts.cols

            Repeater {
                model: opts.shown
                delegate: Rectangle {
                    id: entry
                    required property var modelData
                    required property int index
                    readonly property bool main: !!modelData.main
                    readonly property bool focused: opts.focusIndex === index
                    readonly property bool ringOn: focused && Theme.showFocus && !opts.popover
                    readonly property bool hovered: hover.hovered
                    readonly property bool iconShown: !!modelData.icon && !main && !opts.popover && !Theme.affiche
                    width: grid.cellW
                    height: opts.popover ? Math.max((main ? 36 : 34) * opts.k, label.implicitHeight + 14 * opts.k)
                          : Theme.affiche ? Math.max((main ? 60 : 52) * opts.k, label.implicitHeight + 16 * opts.k)
                          : Math.max((main ? (Theme.ludique ? 62 : 56) : (Theme.ludique ? 54 : 52)) * opts.k, label.implicitHeight + 16 * opts.k)
                    radius: opts.popover ? (main ? Theme.pill : Theme.popRow) * opts.k
                          : (Theme.affiche ? 0 : (Theme.ludique ? height / 2 : 16 * opts.k))
                    color: {
                        if (Theme.affiche && !opts.popover && ringOn) return Theme.ringFillColor
                        if (opts.popover) return main ? Theme.accent : (hovered ? Theme.surface2 : "transparent")
                        if (main) return Theme.affiche ? (Theme.dark ? "#EDEDE8" : "#111111")
                                       : (Theme.braise ? "#FF6A2B" : "#C6F432")
                        if (Theme.affiche) return "transparent"
                        return Theme.braise ? (Theme.dark ? "#24201D" : "#EFE7DD") : (Theme.dark ? "#15112A" : "#F3EEFF")
                    }
                    scale: ringOn && !Theme.affiche ? Theme.focusScale : 1.0
                    Behavior on scale { NumberAnimation { duration: 150 } }

                    // brightness 1.08 on the popover's main entry under the mouse
                    Rectangle { anchors.fill: parent; radius: parent.radius; color: "white"; opacity: opts.popover && entry.main && entry.hovered ? 0.08 : 0 }
                    Rectangle {
                        visible: Theme.affiche && !opts.popover
                        anchors.bottom: parent.bottom
                        width: parent.width; height: opts.k
                        color: Theme.fg
                    }
                    FocusRing { active: entry.ringOn; radius: entry.radius; k: opts.k }

                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        x: opts.popover || Theme.affiche || entry.iconShown ? (opts.popover ? 14 : 16) * opts.k : (parent.width - width) / 2
                        width: Math.min(implicitWidth, parent.width - 2 * (opts.popover ? 14 : 16) * opts.k)
                        spacing: (opts.popover ? 9 : (entry.iconShown ? 12 : 10)) * opts.k
                        // Braise: the icon in the accent; Ludique: in a round chip (the dock's chip colours)
                        Item {
                            id: entryIcon
                            visible: entry.iconShown
                            width: (Theme.ludique ? 32 : 20) * opts.k
                            height: width
                            anchors.verticalCenter: parent.verticalCenter
                            Rectangle {
                                visible: Theme.ludique
                                anchors.fill: parent
                                radius: width / 2
                                color: Theme.dark ? "#2E2650" : "#E4DEFF"
                            }
                            LavIcon {
                                anchors.centerIn: parent
                                path: entry.modelData.icon || ""
                                size: (Theme.ludique ? 17 : 20) * opts.k
                                stroke: Theme.ludique ? 2 : 1.8
                                color: Theme.ludique ? (Theme.dark ? "#F3EEFF" : "#1B1530") : Theme.accentText
                            }
                        }
                        LavIcon {
                            visible: !!entry.modelData.play && !(Theme.affiche && !opts.popover)
                            name: "play"
                            size: (opts.popover ? 12 : 16) * opts.k
                            color: label.color
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            id: label
                            width: Math.min(implicitWidth, entry.width - 2 * (opts.popover ? 14 : 16) * opts.k - (entry.modelData.play ? 26 * opts.k : 0)
                                                           - (entry.iconShown ? entryIcon.width + 12 * opts.k : 0))
                            anchors.verticalCenter: parent.verticalCenter
                            text: (Theme.affiche && !opts.popover && entry.main) ? entry.modelData.l + " →" : entry.modelData.l
                            wrapMode: Text.WordWrap
                            horizontalAlignment: opts.popover || Theme.affiche || entry.iconShown ? Text.AlignLeft : Text.AlignHCenter
                            color: {
                                if (Theme.affiche && !opts.popover && entry.ringOn) return Theme.ringFillFg
                                if (opts.popover) return entry.main ? Theme.onAccent : Theme.fg
                                if (entry.main) return Theme.affiche ? (Theme.dark ? "#141414" : "#ECECE8") : (Theme.braise ? "#0E0D0C" : "#1B1530")
                                return Theme.fg
                            }
                            font.family: Theme.textFont
                            readonly property real px: opts.popover ? 14 : Theme.affiche ? (entry.main ? 26 : 20)
                                                     : Theme.ludique ? (entry.main ? 19 : 17) : (entry.main ? 17 : 16)
                            font.pixelSize: px * opts.k
                            font.weight: opts.popover ? (entry.main ? Font.Bold : Font.Normal)
                                       : Theme.affiche ? (entry.main ? Font.Black : Font.ExtraBold)
                                       : Theme.ludique ? (entry.main ? Font.ExtraBold : Font.Bold)
                                       : (entry.main ? Font.Bold : Font.DemiBold)
                            font.letterSpacing: Theme.affiche && !opts.popover ? (entry.main ? -0.03 : -0.02) * px * opts.k : 0
                        }
                    }
                    HoverHandler {
                        id: hover
                        onHoveredChanged: if (hovered && Theme.input === "mouse") opts.focusIndex = entry.index
                    }
                    TapHandler { onTapped: opts.pick(entry.index) }
                }
            }
        }
    }
}

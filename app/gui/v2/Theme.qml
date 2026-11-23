pragma Singleton

import QtQuick

import ArtemisSettings 1.0

// Design tokens of the redesigned interface: Braise, Affiche and Ludique, each in a light and a
// dark variant. Values come from the validated prototype (docs/ui-redesign/prototype/logic.js in
// LavArtemis). Every v2 component reads its colours, radii and fonts from here, never literals.
QtObject {
    id: theme

    // "braise", "affiche" or "ludique"
    readonly property string id: {
        var t = ArtemisSettings.uiTheme
        return (t === "affiche" || t === "ludique") ? t : "braise"
    }
    // 0 follows the OS (live), 1 light, 2 dark
    readonly property int mode: ArtemisSettings.uiThemeMode
    readonly property bool systemDark: Qt.styleHints.colorScheme === Qt.ColorScheme.Dark
    readonly property bool dark: mode === 2 || (mode === 0 && systemDark)

    // Last input that drove the UI: "pad", "keys", "mouse" or "touch". Gamepad hints only for a
    // real gamepad, keyboard hints for the keyboard, none for the mouse; no focus ring for the mouse.
    property string input: "mouse"
    readonly property bool showFocus: input === "pad" || input === "keys"

    readonly property var palettes: ({
        braise: {
            dark: {bg: "#0E0D0C", surface: "#1A1816", surface2: "#24211E", fg: "#F3EEE8", muted: "#A39A90",
                   accent: "#FF6A2B", onAccent: "#0E0D0C", line: "#2C2724", on: "#FF6A2B", off: "#3A342F",
                   sel: "#2A221C", selFg: "#F3EEE8", badgeBg: "#FF6A2B", badgeFg: "#0E0D0C", shadow: "#000000"},
            light: {bg: "#F6F1EA", surface: "#FFFFFF", surface2: "#EFE7DD", fg: "#1E1A16", muted: "#6B6158",
                    accent: "#E8541A", onAccent: "#FFFFFF", line: "#E2D8CC", on: "#E8541A", off: "#DCD1C4",
                    sel: "#F3E3D6", selFg: "#1E1A16", badgeBg: "#E8541A", badgeFg: "#FFFFFF", shadow: "#3C2814"}
        },
        affiche: {
            light: {bg: "#ECECE8", surface: "#ECECE8", surface2: "#E0E0DA", fg: "#111111", muted: "#555550",
                    accent: "#111111", onAccent: "#ECECE8", line: "#111111", on: "#111111", off: "#D2D2CC",
                    sel: "#111111", selFg: "#ECECE8", badgeBg: "#111111", badgeFg: "#FFD60A", shadow: "#111111",
                    signal: "#FFD60A"},
            dark: {bg: "#141414", surface: "#141414", surface2: "#262624", fg: "#EDEDE8", muted: "#A3A39C",
                   accent: "#EDEDE8", onAccent: "#141414", line: "#EDEDE8", on: "#FFD60A", off: "#3A3A37",
                   sel: "#EDEDE8", selFg: "#141414", badgeBg: "#FFD60A", badgeFg: "#111111", shadow: "#000000",
                   signal: "#FFD60A"}
        },
        ludique: {
            light: {bg: "#F3EEFF", surface: "#FFFFFF", surface2: "#E4DEFF", fg: "#1B1530", muted: "#5A527A",
                    accent: "#C6F432", onAccent: "#1B1530", line: "#E4DEFF", on: "#C6F432", off: "#E4DEFF",
                    sel: "#1B1530", selFg: "#FFFFFF", badgeBg: "#1B1530", badgeFg: "#C6F432", shadow: "#1B1530",
                    coral: "#FF7A59"},
            dark: {bg: "#15112A", surface: "#221C3D", surface2: "#2E2650", fg: "#F3EEFF", muted: "#B3A9D6",
                   accent: "#C6F432", onAccent: "#1B1530", line: "#2E2650", on: "#C6F432", off: "#3A3166",
                   sel: "#C6F432", selFg: "#1B1530", badgeBg: "#C6F432", badgeFg: "#1B1530", shadow: "#000000",
                   coral: "#FF7A59"}
        }
    })

    readonly property var tk: palettes[id][dark ? "dark" : "light"]

    readonly property color bg: tk.bg
    readonly property color surface: tk.surface
    readonly property color surface2: tk.surface2
    readonly property color fg: tk.fg
    readonly property color muted: tk.muted
    readonly property color accent: tk.accent
    readonly property color onAccent: tk.onAccent
    readonly property color line: tk.line
    readonly property color on: tk.on
    readonly property color off: tk.off
    readonly property color sel: tk.sel
    readonly property color selFg: tk.selFg
    readonly property color badgeBg: tk.badgeBg
    readonly property color badgeFg: tk.badgeFg
    readonly property color shadow: tk.shadow
    // The colour that says "running": Braise orange, Affiche yellow, Ludique lime
    readonly property color live: id === "affiche" ? tk.signal : (id === "ludique" ? "#C6F432" : tk.accent)
    readonly property color onLive: id === "braise" ? tk.onAccent : "#111111"

    readonly property int r: id === "affiche" ? 0 : (id === "ludique" ? 28 : 18)
    readonly property int rs: id === "affiche" ? 0 : (id === "ludique" ? 20 : 12)
    readonly property int pill: id === "affiche" ? 0 : (id === "ludique" ? 22 : 14)
    readonly property int border: id === "affiche" ? 3 : (id === "braise" ? 1 : 0)
    readonly property color borderColor: id === "affiche" ? fg : line

    readonly property string displayFont: id === "affiche" ? "Archivo" : (id === "ludique" ? "Bricolage Grotesque" : "Space Grotesk")
    readonly property string textFont: id === "affiche" ? "Archivo" : (id === "ludique" ? "Bricolage Grotesque" : "Inter")
    readonly property int displayWeight: id === "affiche" ? Font.Black : (id === "ludique" ? Font.ExtraBold : Font.Bold)
    readonly property bool upper: id === "affiche"
    readonly property real tracking: id === "affiche" ? -0.02 : (id === "ludique" ? -0.03 : -0.01)

    function displayText(s) { return upper ? String(s).toUpperCase() : s }

    function cycleMode() {
        // Système -> Clair -> Sombre -> Système
        ArtemisSettings.uiThemeMode = (mode + 1) % 3
        ArtemisSettings.save()
    }

    function setTheme(themeId) {
        ArtemisSettings.uiTheme = themeId
        ArtemisSettings.save()
    }

    readonly property var themeNames: ({braise: "Braise", affiche: "Affiche", ludique: "Ludique"})
    readonly property var modeNames: [qsTr("Système"), qsTr("Clair"), qsTr("Sombre")]

    readonly property int animMs: 160

    // A card or cover dimmed while another app runs (prototype dimOff / dimFocus, per theme and
    // mode): grayscale, then brightness, then opacity, applied to the whole tile -- its cover and
    // the name drawn in place of a missing cover alike. Brightness MULTIPLIES like CSS
    // brightness(): MultiEffect.brightness adds, and pushed dark covers to plain black.
    function dim(focused) {
        if (id === "braise") {
            return dark ? (focused ? {gray: 0.35, bright: 0.85, opacity: 1.0} : {gray: 0.9, bright: 0.42, opacity: 1.0})
                        : (focused ? {gray: 0.3, bright: 1.0, opacity: 0.9} : {gray: 0.9, bright: 1.0, opacity: 0.45})
        }
        if (id === "affiche") {
            return focused ? {gray: 0.5, bright: 1.0, opacity: 0.75} : {gray: 1.0, bright: 1.0, opacity: 0.3}
        }
        return dark ? (focused ? {gray: 0.3, bright: 1.0, opacity: 0.95} : {gray: 1.0, bright: 0.7, opacity: 0.55})
                    : (focused ? {gray: 0.4, bright: 1.0, opacity: 0.85} : {gray: 1.0, bright: 1.0, opacity: 0.4})
    }

    // Scale of the 1600-wide Stage of the visible page (set by Stage.qml). Popups live in the window
    // overlay, outside the scaled stage, and use it to keep the same proportions as the page.
    property real stageScale: 1.0

    // Status colours of the PyroWave calibration grid (smooth, tight, very tight, too slow, error):
    // the same in every theme, they carry a meaning.
    readonly property var tierColors: ({any: "#66BB6A", vrr: "#FFCA28", vrrLarge: "#FF9800",
                                        slow: "#EF5350", error: "#9E9E9E"})
}

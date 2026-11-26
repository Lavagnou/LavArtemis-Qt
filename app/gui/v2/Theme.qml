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

    // Shader effects (MultiEffect: blur, masks, glow, dim) exist only with a GPU scene graph. The
    // software one (offscreen captures of the testbench, `testbench.py shots`) draws nothing for an
    // item with such an effect: the pages then draw without it (square corners, no blur, no glow).
    property bool effects: true

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

    // Mouse hover on a card or cover (prototype `ringMouse`): it lifts by 5 px with a soft shadow and
    // a thin outline, and counts as the focus for the dimming (colour comes back). Gamepad/keyboard
    // focus instead scales it to 1.04 with the full focus ring (prototype `ring`).
    readonly property color hoverRing: id === "braise" ? (dark ? Qt.rgba(1, 0.416, 0.169, 0.6) : Qt.rgba(0.91, 0.329, 0.102, 0.6))
                                     : (id === "ludique" ? (dark ? "#C6F432" : "#1B1530") : fg)
    readonly property int hoverRingWidth: id === "braise" ? 2 : 3
    readonly property real hoverShadowOpacity: dark ? 0.55 : 0.22
    readonly property real focusScale: 1.04

    // ---- prototype V8 tokens (design-port-spec §1.1) ------------------------------------------
    // Values are prototype px; the pages multiply by 1.25 where they are laid out in 1600 units.
    readonly property bool braise: id === "braise"
    readonly property bool affiche: id === "affiche"
    readonly property bool ludique: id === "ludique"
    readonly property color switchKnobOn: braise ? (dark ? "#0E0D0C" : "#FFFFFF") : (affiche ? (dark ? "#111111" : "#ECECE8") : "#1B1530")
    readonly property color switchKnobOff: braise ? (dark ? "#CFC6BC" : "#6B6158") : (affiche ? (dark ? "#EDEDE8" : "#111111") : (dark ? "#B3A9D6" : "#5A527A"))
    readonly property color warnBg: braise ? (dark ? "#2A1A12" : "#FBE3D4") : (affiche ? (dark ? "#3B330F" : "#FFD60A") : (dark ? "#3A2430" : "#FFE1D8"))
    readonly property color warnLine: braise ? accent : (affiche ? (dark ? "#FFD60A" : "#111111") : "#FF7A59")
    // Secondary text on the Braise hero (#E7E0D8 / #3E3731), Braise accent text (#FF8A57 / #C9480F)
    readonly property color text2: braise ? (dark ? "#E7E0D8" : "#3E3731") : muted
    readonly property color accentText: braise ? (dark ? "#FF8A57" : "#C9480F") : (affiche ? fg : (dark ? "#C6F432" : "#3F5A00"))
    // Focus ring (prototype `ring`): a gap in the background colour, then the ring. Braise 3 + 3 px
    // accent; Ludique 4 + 5 px navy (light) or lime (dark); Affiche fills in yellow + 4 px outline.
    readonly property real ringGap: ludique ? 4 : 3
    readonly property real ringWidth: ludique ? 5 : 3
    readonly property color ringColor: ludique ? (dark ? "#C6F432" : "#1B1530") : accent
    readonly property bool ringFill: affiche
    readonly property color ringFillColor: "#FFD60A"
    readonly property color ringFillFg: "#111111"
    readonly property color ring2Color: dark ? "#EDEDE8" : "#111111"
    // Running glow (lavember / lavplum / lavbar + lavpop), period of one breath (ms)
    readonly property color glowColor: ludique ? "#7B5CFF" : (affiche ? "#FFD60A" : "#FF6A2B")
    readonly property int glowMs: braise ? 2600 : (ludique ? 2400 : 1600)
    // Hover shadow (prototype ringMouse) per theme and mode
    readonly property color hoverShadow: braise ? (dark ? Qt.rgba(0, 0, 0, 0.55) : Qt.rgba(60/255, 40/255, 20/255, 0.22))
                                                : (dark ? Qt.rgba(0, 0, 0, 0.45) : Qt.rgba(27/255, 21/255, 48/255, 0.25))
    readonly property int popR: affiche ? 0 : (ludique ? 24 : 16)
    readonly property int popRow: affiche ? 0 : (ludique ? 16 : 10)
    readonly property int thumbR: affiche ? 0 : (ludique ? 10 : 8)
    // Inverse of the page (toasts, keycaps on Braise)
    readonly property color inverse: fg
    readonly property color inverseInk: bg

    // The prototype's own icons (24 x 24 SVG paths, for LavIcon.path)
    readonly property var icons: ({
        moon: "M12 3a6 6 0 0 0 9 9 9 9 0 1 1-9-9z",
        sun: "M12 8a4 4 0 1 0 0 8 4 4 0 0 0 0-8zM12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4",
        multi: "M3.5 5h9a1.5 1.5 0 0 1 1.5 1.5v6a1.5 1.5 0 0 1-1.5 1.5h-9A1.5 1.5 0 0 1 2 12.5v-6A1.5 1.5 0 0 1 3.5 5zM11.5 9h9a1.5 1.5 0 0 1 1.5 1.5v6a1.5 1.5 0 0 1-1.5 1.5h-9a1.5 1.5 0 0 1-1.5-1.5v-6A1.5 1.5 0 0 1 11.5 9z",
        screen: "M3 4h18v12H3zM8 20h8M12 16v4",
        gear: "M15 12a3 3 0 1 1-6 0 3 3 0 0 1 6 0zM19.4 15a1.7 1.7 0 0 0 .3 1.8l.1.1a2 2 0 1 1-2.8 2.8l-.1-.1a1.7 1.7 0 0 0-1.8-.3 1.7 1.7 0 0 0-1 1.5V21a2 2 0 1 1-4 0v-.1a1.7 1.7 0 0 0-1.1-1.5 1.7 1.7 0 0 0-1.8.3l-.1.1a2 2 0 1 1-2.8-2.8l.1-.1a1.7 1.7 0 0 0 .3-1.8 1.7 1.7 0 0 0-1.5-1H3a2 2 0 1 1 0-4h.1a1.7 1.7 0 0 0 1.5-1.1 1.7 1.7 0 0 0-.3-1.8l-.1-.1a2 2 0 1 1 2.8-2.8l.1.1a1.7 1.7 0 0 0 1.8.3H9a1.7 1.7 0 0 0 1-1.5V3a2 2 0 1 1 4 0v.1a1.7 1.7 0 0 0 1 1.5 1.7 1.7 0 0 0 1.8-.3l.1-.1a2 2 0 1 1 2.8 2.8l-.1.1a1.7 1.7 0 0 0-.3 1.8V9a1.7 1.7 0 0 0 1.5 1H21a2 2 0 1 1 0 4h-.1a1.7 1.7 0 0 0-1.5 1z",
        update: "M20 11a8 8 0 1 0-1.7 5M20 5v6h-6",
        play: "M8 5l11 7-11 7z",
        grid: "M4 4h7v7H4zM13 4h7v7h-7zM4 13h7v7H4zM13 13h7v7h-7z"
    })

    // Scale of the 1600-wide Stage of the visible page (set by Stage.qml). Popups live in the window
    // overlay, outside the scaled stage, and use it to keep the same proportions as the page.
    property real stageScale: 1.0

    // Status colours of the PyroWave calibration grid (smooth, tight, very tight, too slow, error):
    // the same in every theme, they carry a meaning.
    readonly property var tierColors: ({any: "#66BB6A", vrr: "#FFCA28", vrrLarge: "#FF9800",
                                        slow: "#EF5350", error: "#9E9E9E"})
}

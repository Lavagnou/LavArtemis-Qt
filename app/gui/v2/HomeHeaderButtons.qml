import QtQuick

// Right group of the home header, every theme (prototype): mode (light / dark), Multi-écran (PC),
// Mes PC, Réglages; 8 px apart. Only Mes PC (nh) and Réglages (ns) are in the focus grid.
Row {
    id: group
    property var home: null
    property real k: 1.25
    spacing: 8 * k

    HeaderButton {
        home: group.home
        k: group.k
        iconPath: Theme.dark ? Theme.icons.moon : Theme.icons.sun
        iconStroke: 1.9
        badge: Theme.mode === 0 ? "A" : ""
        tip: group.home ? group.home.modeTitle : ""
        onClicked: if (group.home) group.home.cycleMode()
    }
    HeaderButton {
        objectName: "multiScreenPill"
        home: group.home
        k: group.k
        iconPath: Theme.icons.multi
        text: qsTr("Multi-écran")
        showSwitch: true
        switchOn: group.home ? group.home.multiOn : false
        tip: group.home ? group.home.multiTip : ""
        onClicked: if (group.home) group.home.toggleMulti()
    }
    HeaderButton {
        home: group.home
        fid: "nh"
        k: group.k
        iconPath: Theme.icons.screen
        text: qsTr("Mes PC")
        onClicked: if (group.home) group.home.act("nh")
    }
    HeaderButton {
        home: group.home
        fid: "ns"
        k: group.k
        iconPath: Theme.icons.gear
        text: qsTr("Réglages")
        onClicked: if (group.home) group.home.act("ns")
    }
}

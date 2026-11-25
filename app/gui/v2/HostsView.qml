import QtQuick
import QtQuick.Controls
import QtQuick.Shapes

import ComputerModel 1.0
import ComputerManager 1.0
import SdlGamepadKeyNavigation 1.0

// "Mes PC" (prototype V4, board "hosts"): a card per paired host with its state, the address in
// use and what runs on it; then "Ajouter un PC" (hosts found on the network, or by address).
// Click: open the host on the home (or wake/pair it). Right click, "⋯", Menu/Y: open, benchmark,
// wake, rename, forget.
FocusScope {
    id: hostsPage
    objectName: qsTr("Mes PC")
    focus: true

    property var home: null
    property ComputerModel computerModel: createModel()

    // Bumped on every model change: the bindings below read the model through data()
    property int rev: 0
    readonly property int pairedCount: { rev; return countHosts(true) }
    readonly property int cols: Math.max(2, Math.min(5, pairedCount + 1))

    // Pairing under way (index, uuid and name of the host), and the hosts paired before a manual add
    property int pairingIndex: -1
    property string pairingUuid: ""
    property string pairingName: ""
    property var pairedBeforeAdd: []
    property string addingAddress: ""

    function createModel() {
        var model = Qt.createQmlObject('import ComputerModel 1.0; ComputerModel {}', hostsPage, '')
        model.initialize(ComputerManager)
        model.pairingCompleted.connect(pairingComplete)
        model.rowsInserted.connect(function() { hostsPage.rev++ })
        model.rowsRemoved.connect(function() { hostsPage.rev++ })
        model.modelReset.connect(function() { hostsPage.rev++ })
        model.dataChanged.connect(function() { hostsPage.rev++ })
        return model
    }

    function role(i, name) {
        return computerModel.data(computerModel.index(i, 0), roleId(name))
    }
    // ComputerModel::Roles is not exported to QML: the ids follow its order from Qt::UserRole.
    // Keep this list in step with computermodel.h.
    function roleId(name) {
        var names = ["name", "online", "paired", "busy", "wakeable", "statusUnknown", "serverSupported",
                     "apolloVersion", "uuid", "address", "runningApp", "serverVersion", "thisPc"]
        return 256 + names.indexOf(name) // Qt::UserRole + position in ComputerModel::Roles
    }

    function countHosts(paired) {
        var n = 0
        for (var i = 0; i < computerModel.rowCount(); i++) {
            if (role(i, "paired") === paired) n++
        }
        return n
    }

    function indexOfUuid(uuid) {
        for (var i = 0; i < computerModel.rowCount(); i++) {
            if (role(i, "uuid") === uuid) return i
        }
        return -1
    }

    // ---- actions ------------------------------------------------------------------------------
    // « Logiciel hôte non pris en charge » (NvComputer::isSupportedServerVersion, Android's HostSupport):
    // asked before opening, benchmarking and launching, with Android's message and the version
    function checkSupported(i) {
        if (role(i, "serverSupported")) return true
        showError(qsTr("La version du logiciel hôte de %1 n'est pas prise en charge (%2). Mets à jour LavApollo sur ce PC.")
                  .arg(role(i, "name")).arg(role(i, "serverVersion")))
        return false
    }

    function openHost(i) {
        if (!checkSupported(i)) return
        if (home) home.showHost(role(i, "uuid"))
        stackView.pop()
    }

    function wakeHost(i) {
        if (role(i, "wakeable")) {
            computerModel.wakeComputer(i)
            toast.show(qsTr("Réveil envoyé à %1").arg(role(i, "name")))
        } else {
            showError(qsTr("%1 ne peut pas être réveillé d'ici : il n'a encore jamais donné son adresse réseau matérielle (MAC). Allume-le une fois à la main pendant que LavArtemis tourne.").arg(role(i, "name")))
        }
    }

    function pairHost(i) {
        var pin = computerModel.generatePinString()
        pairingIndex = i
        pairingUuid = role(i, "uuid")
        pairingName = role(i, "name")
        computerModel.pairComputer(i, pin)
        pairDialog.pin = pin
        pairDialog.open()
    }

    function activate(i) {
        var online = role(i, "online"), paired = role(i, "paired")
        if (online && paired) openHost(i)
        else if (online) pairHost(i)
        else if (!role(i, "statusUnknown")) wakeHost(i)
    }

    function pairingComplete(error) {
        pairDialog.close()
        if (error !== undefined) {
            showError(error)
            return
        }
        toast.show(qsTr("%1 est appairé").arg(pairingName))
        var uuid = pairingUuid
        pairingIndex = -1
        if (home && uuid !== "") {
            home.showHost(uuid)
            stackView.pop()
        }
    }

    function openHostMenu(item, i, x, y) {
        var online = role(i, "online"), paired = role(i, "paired")
        var list = []
        if (online && paired) {
            list.push({id: "open", label: qsTr("Ouvrir"), icon: "play", main: true})
            list.push({id: "bench", label: qsTr("Benchmark des réglages"), icon: "gauge"})
        } else if (online) {
            list.push({id: "pair", label: qsTr("Appairer"), icon: "lock", main: true})
        } else {
            list.push({id: "wake", label: qsTr("Réveiller"), icon: "power", main: true})
        }
        list.push({id: "rename", label: qsTr("Renommer"), icon: "pencil"})
        list.push({id: "forget", label: qsTr("Oublier ce PC"), icon: "trash"})
        hostMenu.hostIndex = i
        hostMenu.appName = role(i, "name")
        hostMenu.customEntries = list
        var p = item.mapToItem(hostsPage, x, y)
        hostMenu.openAt(p.x, p.y)
    }

    function onHostMenu(action) {
        var i = hostMenu.hostIndex
        if (i < 0 || i >= computerModel.rowCount()) return
        if (action === "open") openHost(i)
        else if (action === "pair") pairHost(i)
        else if (action === "wake") wakeHost(i)
        else if (action === "bench") {
            if (!checkSupported(i)) return
            if (role(i, "thisPc")) {
                // A benchmark streams: same refusal as a launch (Session::validateLaunch)
                showError(qsTr("Impossible de streamer %1 depuis lui-même : LavApollo tourne sur ce PC. Lance le stream depuis un autre appareil.").arg(role(i, "name")))
                return
            }
            var component = Qt.createComponent("../BenchmarkView.qml")
            stackView.push(component.createObject(stackView, {"hostUuid": role(i, "uuid"), "hostName": role(i, "name")}))
        } else if (action === "rename") {
            renameDialog.hostIndex = i
            renameField.text = role(i, "name")
            renameDialog.open()
        } else if (action === "forget") {
            forgetDialog.hostIndex = i
            forgetDialog.hostName = role(i, "name")
            forgetDialog.open()
        }
    }

    function openAddMenu(item, x, y) {
        var list = []
        for (var i = 0; i < computerModel.rowCount(); i++) {
            if (!role(i, "paired") && role(i, "online")) {
                var addr = role(i, "address")
                list.push({id: "pair:" + i, label: role(i, "name") + (addr ? " · " + addr : ""), icon: "monitor", main: list.length === 0})
            }
        }
        list.push({id: "manual", label: qsTr("Saisir une adresse…"), icon: "search", main: list.length === 0})
        addMenu.customEntries = list
        addMenu.appName = list.length > 1 ? qsTr("Trouvés sur le réseau") : qsTr("Aucun PC trouvé sur le réseau")
        var p = item.mapToItem(hostsPage, x, y)
        addMenu.openAt(p.x, p.y)
    }

    function onAddMenu(action) {
        if (action.indexOf("pair:") === 0) {
            pairHost(parseInt(action.slice(5)))
        } else if (action === "manual") {
            addressField.text = ""
            addDialog.open()
        }
    }

    function addByAddress(address) {
        address = address.trim()
        if (address.length === 0) return
        // Test build only: a refused address (not a loopback test host) never starts a search
        var refusal = ComputerManager.testGuardRefusal(address)
        if (refusal !== "") {
            showError(refusal)
            return
        }
        var paired = []
        for (var i = 0; i < computerModel.rowCount(); i++) {
            if (role(i, "paired")) paired.push(role(i, "uuid"))
        }
        pairedBeforeAdd = paired
        addingAddress = address
        toast.show(qsTr("Recherche de %1…").arg(address))
        ComputerManager.addNewHostManually(address)
    }

    // uuid = the host that answered at this address, new or already known. ComputerManager emits
    // this after the state change that put a new host in the model, so its row is there.
    // Same outcome as Android's HostsActivity.onAdded(): not paired -> pair it now; paired
    // before -> already in the list; otherwise added.
    function addComplete(success, detectedPortBlocking, uuid) {
        if (addingAddress === "") return
        var address = addingAddress
        addingAddress = ""
        if (!success) {
            var text = qsTr("Impossible de joindre %1. Vérifie que le PC est allumé, que LavApollo tourne et que l'adresse est la bonne.").arg(address)
            if (detectedPortBlocking) {
                text += "\n\n" + qsTr("La connexion Internet de ce réseau semble bloquer LavArtemis : le stream par Internet risque de ne pas marcher d'ici.")
            }
            showError(text)
            return
        }
        var i = indexOfUuid(uuid)
        if (i < 0) {
            // Forgotten meanwhile (it can't be missing otherwise): say so, the add did nothing visible
            console.warn("Added host", uuid, "is not in the list")
            toast.show(qsTr("%1 a été oublié pendant l'ajout : ajoute-le à nouveau").arg(address))
            return
        }
        if (!role(i, "paired")) pairHost(i)
        else if (pairedBeforeAdd.indexOf(uuid) >= 0) toast.show(qsTr("Ce PC est déjà dans la liste"))
        else toast.show(qsTr("%1 est ajouté").arg(role(i, "name")))
    }

    function showError(text) {
        errorDialog.text = text
        errorDialog.open()
    }

    // Arrow keys / d-pad across the grid (cards, then "Ajouter un PC")
    function moveFocus(item, delta) {
        var items = []
        for (var i = 0; i < grid.children.length; i++) {
            var c = grid.children[i]
            if (c.visible && c.focusPolicy !== undefined && c.focusPolicy !== Qt.NoFocus) items.push(c)
        }
        var at = items.indexOf(item)
        var next = at + delta
        if (at < 0 || next < 0 || next >= items.length) return
        items[next].forceActiveFocus()
    }

    StackView.onActivated: {
        ComputerManager.computerAddCompleted.connect(addComplete)
        if (Theme.input === "mouse" && SdlGamepadKeyNavigation.getConnectedGamepads() > 0) Theme.input = "pad"
    }
    StackView.onDeactivating: ComputerManager.computerAddCompleted.disconnect(addComplete)

    Connections {
        target: SdlGamepadKeyNavigation
        function onGamepadInput() { Theme.input = "pad" }
    }
    HoverHandler {
        onPointChanged: if (Theme.input !== "mouse" && point.device.type === PointerDevice.Mouse) Theme.input = "mouse"
    }
    Keys.onPressed: (event) => {
        if (Theme.input !== "keys" && Theme.input !== "pad") Theme.input = "keys"
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) { stackView.pop(); event.accepted = true }
    }

    Rectangle { anchors.fill: parent; color: Theme.bg }

    Stage {
        anchors.fill: parent

        Flickable {
            anchors.fill: parent
            contentHeight: column.implicitHeight + 60
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            ScrollBar.vertical: ScrollBar {}

            Column {
                id: column
                x: 50; y: 6 // Close under the title bar, which is empty and in the theme's colours (see main.qml themedFrame)
                width: parent.width - 100
                spacing: 30

                Item {
                    width: parent.width
                    height: header.height
                    PageHeader {
                        id: header
                        title: qsTr("Mes PC")
                        onBack: stackView.pop()
                    }
                    Text {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Appairés avec cet appareil")
                        color: Theme.muted
                        font.family: Theme.textFont
                        font.pixelSize: 18
                    }
                }

                Flow {
                    id: grid
                    width: parent.width
                    spacing: 20
                    readonly property real cardWidth: (width - spacing * (hostsPage.cols - 1)) / hostsPage.cols

                    Repeater {
                        model: hostsPage.computerModel
                        delegate: AbstractButton {
                            id: card
                            required property int index
                            required property string name
                            required property bool online
                            required property bool paired
                            required property bool busy
                            required property bool wakeable
                            required property bool statusUnknown
                            required property string apolloVersion
                            required property string address
                            required property string runningApp

                            // Unpaired hosts are offered by "Ajouter un PC" instead
                            visible: paired
                            width: grid.cardWidth
                            height: 470
                            hoverEnabled: true
                            focusPolicy: Qt.StrongFocus

                            readonly property string status: statusUnknown ? qsTr("Recherche…")
                                : (!online ? qsTr("Hors ligne") : (busy ? qsTr("Session en cours") : qsTr("En ligne")))
                            readonly property string detail: online
                                ? address + (apolloVersion.length > 0 ? " · LavApollo " + apolloVersion : "")
                                : (statusUnknown ? "" : (wakeable ? qsTr("Éteint ou en veille : il peut être réveillé d'ici.")
                                                                  : qsTr("Éteint ou injoignable.")))
                            readonly property string actionLabel: online ? qsTr("Ouvrir") : (statusUnknown ? "" : qsTr("Réveiller"))

                            onClicked: hostsPage.activate(index)
                            Keys.onReturnPressed: hostsPage.activate(index)
                            Keys.onEnterPressed: hostsPage.activate(index)
                            Keys.onMenuPressed: hostsPage.openHostMenu(card, index, width / 2, height / 2)
                            Keys.onLeftPressed: hostsPage.moveFocus(card, -1)
                            Keys.onRightPressed: hostsPage.moveFocus(card, 1)
                            Keys.onUpPressed: hostsPage.moveFocus(card, -hostsPage.cols)
                            Keys.onDownPressed: hostsPage.moveFocus(card, hostsPage.cols)

                            // Prototype: mouse hover lifts the card (shadow + thin outline), gamepad/keyboard focus scales it;
                            // both bring the colour back on a dimmed card (Theme.dim(true))
                            readonly property bool mouseHover: hovered && Theme.input === "mouse"
                            readonly property bool padFocus: activeFocus && Theme.showFocus
                            transform: Translate {
                                y: card.mouseHover ? -5 : 0
                                Behavior on y { NumberAnimation { duration: 150; easing.type: Easing.OutQuad } }
                            }
                            scale: card.padFocus ? Theme.focusScale : 1.0
                            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutQuad } }

                            HoverDecor {
                                anchors.fill: parent
                                active: card.mouseHover
                                radius: Theme.r
                            }
                            Rectangle {
                                // Thin outline on mouse hover (the full focus ring is for the gamepad and keyboard)
                                anchors.fill: parent
                                radius: Theme.r
                                color: "transparent"
                                border.width: Theme.hoverRingWidth
                                border.color: Theme.hoverRing
                                visible: card.mouseHover
                                z: 3
                            }

                            background: Rectangle {
                                radius: Theme.r
                                color: Theme.surface
                                border.width: Theme.border
                                border.color: Theme.borderColor
                            }

                            contentItem: Item {
                                Column {
                                    x: 30; y: 32
                                    width: parent.width - 60
                                    spacing: 12

                                    Rectangle {
                                        width: 80; height: 80
                                        radius: Theme.rs
                                        color: Theme.surface2
                                        LavIcon { anchors.centerIn: parent; name: "monitor"; size: 38; stroke: 1.8; color: Theme.fg }
                                    }
                                    Item { width: 1; height: 4 }
                                    Text {
                                        width: parent.width
                                        text: Theme.displayText(card.name)
                                        color: Theme.fg
                                        elide: Text.ElideRight
                                        font.family: Theme.displayFont
                                        font.pixelSize: 32
                                        font.weight: Theme.displayWeight
                                        font.letterSpacing: Theme.tracking * 32
                                    }
                                    Row {
                                        spacing: 10
                                        Rectangle {
                                            width: 11; height: 11; radius: 6
                                            anchors.verticalCenter: parent.verticalCenter
                                            color: card.online ? "#3DDC84" : "#8A8178"
                                        }
                                        Text {
                                            text: card.status
                                            color: Theme.fg
                                            font.family: Theme.textFont
                                            font.pixelSize: 18
                                            font.weight: Font.DemiBold
                                        }
                                    }
                                    Text {
                                        width: parent.width
                                        visible: text.length > 0
                                        text: card.detail
                                        color: Theme.muted
                                        wrapMode: Text.Wrap
                                        maximumLineCount: 3
                                        elide: Text.ElideRight
                                        font.family: Theme.textFont
                                        font.pixelSize: 17
                                        lineHeight: 1.2
                                    }
                                    Rectangle {
                                        visible: card.busy
                                        width: Math.min(parent.width, runText.implicitWidth + 28)
                                        height: 34
                                        radius: Theme.pill
                                        color: Theme.accent
                                        Text {
                                            id: runText
                                            anchors.centerIn: parent
                                            width: Math.min(implicitWidth, parent.width - 28)
                                            elide: Text.ElideRight
                                            text: card.runningApp.length > 0 ? qsTr("En cours : %1").arg(card.runningApp) : qsTr("En cours")
                                            color: Theme.onAccent
                                            font.family: Theme.textFont
                                            font.pixelSize: 15
                                            font.weight: Font.Bold
                                        }
                                    }
                                }

                                Rectangle {
                                    // Main action, at the bottom
                                    visible: card.actionLabel.length > 0
                                    x: 30
                                    anchors.bottom: parent.bottom
                                    anchors.bottomMargin: 30
                                    width: actionText.implicitWidth + 44
                                    height: 52
                                    radius: Theme.pill
                                    color: Theme.fg
                                    Text {
                                        id: actionText
                                        anchors.centerIn: parent
                                        text: card.actionLabel
                                        color: Theme.bg
                                        font.family: Theme.textFont
                                        font.pixelSize: 18
                                        font.weight: Font.Bold
                                    }
                                }

                                AbstractButton {
                                    id: moreButton
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.margins: 18
                                    width: 44; height: 44
                                    visible: (card.hovered && Theme.input === "mouse") || Theme.input === "touch"
                                    focusPolicy: Qt.NoFocus
                                    background: Rectangle { radius: Theme.rs; color: Theme.surface2 }
                                    contentItem: LavIcon { name: "dots"; color: Theme.fg; size: 20; anchors.centerIn: parent }
                                    onClicked: hostsPage.openHostMenu(card, card.index, moreButton.x, moreButton.y + moreButton.height)
                                }
                            }

                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: -6
                                radius: Theme.r + 6
                                color: "transparent"
                                border.width: 3
                                border.color: Theme.accent
                                visible: card.activeFocus && Theme.showFocus
                            }

                            TapHandler {
                                acceptedButtons: Qt.RightButton
                                onTapped: (eventPoint) => {
                                    Theme.input = "mouse"
                                    hostsPage.openHostMenu(card, card.index, eventPoint.position.x, eventPoint.position.y)
                                }
                            }
                            TapHandler {
                                acceptedDevices: PointerDevice.TouchScreen
                                onLongPressed: {
                                    Theme.input = "touch"
                                    hostsPage.openHostMenu(card, card.index, card.width / 2, card.height / 2)
                                }
                            }
                        }
                    }

                    // "Ajouter un PC": dashed card
                    AbstractButton {
                        id: addCard
                        width: grid.cardWidth
                        height: 470
                        hoverEnabled: true
                        focusPolicy: Qt.StrongFocus
                        onClicked: hostsPage.openAddMenu(addCard, width / 2, height / 2)
                        Keys.onReturnPressed: clicked()
                        Keys.onEnterPressed: clicked()
                        Keys.onLeftPressed: hostsPage.moveFocus(addCard, -1)
                        Keys.onUpPressed: hostsPage.moveFocus(addCard, -hostsPage.cols)
                        // Prototype: mouse hover lifts the card (shadow + thin outline), gamepad/keyboard focus scales it;
                        // both bring the colour back on a dimmed card (Theme.dim(true))
                        readonly property bool mouseHover: hovered && Theme.input === "mouse"
                        readonly property bool padFocus: activeFocus && Theme.showFocus
                        transform: Translate {
                            y: addCard.mouseHover ? -5 : 0
                            Behavior on y { NumberAnimation { duration: 150; easing.type: Easing.OutQuad } }
                        }
                        scale: addCard.padFocus ? Theme.focusScale : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutQuad } }

                        HoverDecor {
                            anchors.fill: parent
                            active: addCard.mouseHover
                            radius: Theme.r
                        }
                        Rectangle {
                            // Thin outline on mouse hover (the full focus ring is for the gamepad and keyboard)
                            anchors.fill: parent
                            radius: Theme.r
                            color: "transparent"
                            border.width: Theme.hoverRingWidth
                            border.color: Theme.hoverRing
                            visible: addCard.mouseHover
                            z: 3
                        }

                        background: Shape {
                            preferredRendererType: Shape.CurveRenderer
                            ShapePath {
                                strokeColor: Theme.muted
                                strokeWidth: 2
                                strokeStyle: ShapePath.DashLine
                                dashPattern: [4, 3]
                                fillColor: "transparent"
                                PathRectangle {
                                    x: 1; y: 1
                                    width: addCard.width - 2; height: addCard.height - 2
                                    radius: Theme.r
                                }
                            }
                        }
                        contentItem: Item {
                          Column {
                            anchors.centerIn: parent
                            spacing: 14
                            LavIcon { anchors.horizontalCenter: parent.horizontalCenter; name: "plus"; size: 48; stroke: 1.8; color: Theme.fg }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: Theme.displayText(qsTr("Ajouter un PC"))
                                color: Theme.fg
                                font.family: Theme.displayFont
                                font.pixelSize: 27
                                font.weight: Theme.displayWeight
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: Math.min(250, addCard.width - 40)
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.Wrap
                                text: qsTr("Trouvé sur le réseau, ou par son adresse")
                                color: Theme.muted
                                font.family: Theme.textFont
                                font.pixelSize: 17
                            }
                          }
                        }

                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: -6
                            radius: Theme.r + 6
                            color: "transparent"
                            border.width: 3
                            border.color: Theme.accent
                            visible: addCard.activeFocus && Theme.showFocus
                        }
                    }
                }

                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    color: Theme.muted
                    font.family: Theme.textFont
                    font.pixelSize: 17
                    text: Theme.input === "pad" ? qsTr("A : ouvrir le PC. Y ou A maintenu : réveiller, benchmark, renommer, oublier.")
                        : Theme.input === "keys" ? qsTr("Entrée : ouvrir le PC. Touche Menu : réveiller, benchmark, renommer, oublier.")
                        : Theme.input === "touch" ? qsTr("Touche pour ouvrir le PC. Appui long ou bouton « … » : réveiller, benchmark, renommer, oublier.")
                        : qsTr("Clic : ouvrir le PC. Clic droit ou bouton « … » : réveiller, benchmark, renommer, oublier.")
                }
            }
        }
    }

    Component.onCompleted: {
        // Focus the first card (or "Ajouter un PC" when there is none)
        Qt.callLater(function() {
            for (var i = 0; i < grid.children.length; i++) {
                var c = grid.children[i]
                if (c.visible && c.focusPolicy === Qt.StrongFocus) { c.forceActiveFocus(); return }
            }
        })
    }

    AppMenu {
        id: hostMenu
        property int hostIndex: -1
        parent: hostsPage
        onTriggered: (action) => hostsPage.onHostMenu(action)
    }

    AppMenu {
        id: addMenu
        parent: hostsPage
        onTriggered: (action) => hostsPage.onAddMenu(action)
    }

    LavDialog {
        id: pairDialog
        property string pin: "0000"
        title: qsTr("Appairer %1").arg(hostsPage.pairingName)
        text: qsTr("Sur %1, ouvre LavApollo › Appairage et entre ce code : %2. Il n'est demandé qu'une fois ; cette fenêtre se fermera toute seule.")
                .arg(hostsPage.pairingName).arg(pin.split("").join(" "))
        acceptText: qsTr("C'est fait : le code est entré")
        rejectText: ""
    }

    LavDialog {
        id: addDialog
        title: qsTr("Ajouter un PC")
        text: qsTr("Adresse IP ou nom du PC hôte (par exemple 192.168.1.24).")
        acceptText: qsTr("Ajouter")
        acceptEnabled: addressField.text.trim().length > 0
        initialFocus: addressField.input
        fields: [
            LavTextField {
                id: addressField
                width: addDialog.bodyWidth
                placeholderText: "192.168.1.24"
                onAccepted: addDialog.accept()
            }
        ]
        onAccepted: hostsPage.addByAddress(addressField.text)
    }

    LavDialog {
        id: renameDialog
        property int hostIndex: -1
        title: qsTr("Renommer")
        text: qsTr("Le nom de ce PC dans LavArtemis. Le PC lui-même garde le sien.")
        acceptText: qsTr("Renommer")
        acceptEnabled: renameField.text.trim().length > 0
        initialFocus: renameField.input
        fields: [
            LavTextField {
                id: renameField
                width: renameDialog.bodyWidth
                maximumLength: 64
                onAccepted: renameDialog.accept()
            }
        ]
        onAccepted: hostsPage.computerModel.renameComputer(hostIndex, renameField.text.trim())
    }

    LavDialog {
        id: forgetDialog
        property int hostIndex: -1
        property string hostName: ""
        title: qsTr("Oublier %1 ?").arg(hostName)
        text: qsTr("Il disparaît de LavArtemis. Pour le retrouver, il faudra l'ajouter et l'appairer à nouveau.")
        acceptText: qsTr("Oublier")
        onAccepted: hostsPage.computerModel.deleteComputer(hostIndex)
    }

    LavDialog {
        id: errorDialog
        title: qsTr("Oups")
        rejectText: ""
    }

    Rectangle {
        id: toast
        function show(message) { toastText.text = message; opacity = 1; toastTimer.restart() }
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 40
        width: toastText.implicitWidth + 40; height: 48
        radius: Theme.pill
        color: Theme.fg
        opacity: 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 200 } }
        Text { id: toastText; anchors.centerIn: parent; color: Theme.bg; font.family: Theme.textFont; font.pixelSize: 16 }
        Timer { id: toastTimer; interval: 2500; onTriggered: toast.opacity = 0 }
    }
}

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

    // The pairing this page started (uuid and name of the host): only this page reacts to its end
    // (an earlier Mes PC page can still be alive), and its dialog shows that host even when the
    // list moves. Manual adds under way: typed address -> uuids paired before it (Android's
    // pairedBefore), so overlapping adds each complete with their own address.
    property string pairingUuid: ""
    property string pairingName: ""
    property var pendingAdds: ({})

    function createModel() {
        var model = Qt.createQmlObject('import ComputerModel 1.0; ComputerModel {}', hostsPage, '')
        // The prototype's order: the PCs that answer first (a card moves when its PC comes online)
        model.onlineFirst = true
        model.initialize(ComputerManager)
        model.pairingCompleted.connect(pairingComplete)
        model.wakeCompleted.connect(wakeComplete)
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
            // The toast comes when the packets left, or could not (Android's v2_wake_sent / v2_wake_failed)
            computerModel.wakeComputer(i)
        } else {
            showError(qsTr("%1 ne peut pas être réveillé d'ici : il n'a encore jamais donné son adresse réseau matérielle (MAC). Allume-le une fois à la main pendant que LavArtemis tourne.").arg(role(i, "name")))
        }
    }

    function wakeComplete(uuid, name, sent) {
        toast.show(sent ? qsTr("Réveil envoyé à %1").arg(name)
                        : qsTr("Le réveil de %1 n'a pas pu être envoyé.").arg(name))
    }

    // One pairing at a time (Android's HostsActivity.pair): while one runs, its dialog comes back
    // (closing it with « C'est fait » does not stop it; it ends when the PC answers, or after
    // 3 minutes without the code)
    function pairHost(i) {
        var busy = ComputerManager.pairingUuid()
        if (busy !== "") {
            var b = indexOfUuid(busy)
            pairingUuid = busy
            pairingName = b >= 0 ? role(b, "name") : pairingName
            showPairing(ComputerManager.pairingPin())
            return
        }
        if (!role(i, "online")) {
            showError(qsTr("%1 est hors ligne : allume-le, puis appaire-le.").arg(role(i, "name")))
            return
        }
        var pin = computerModel.generatePinString()
        if (!computerModel.pairComputer(i, pin)) return
        pairingUuid = role(i, "uuid")
        pairingName = role(i, "name")
        showPairing(pin)
    }
    // Planche V8-39: the code in big digits; « Annuler » only closes the dialog (the pairing goes on
    // until the PC answers, or 3 minutes without the code), as « C'est fait » did
    function showPairing(pin) {
        pinBox.pin = pin.split("").join(" ")
        showOptions("pair", {title: qsTr("Appairer %1").arg(pairingName), head: headLine(pairingName), info: true,
                             note: qsTr("Sur %1, ouvre LavApollo › Appairage et entre ce code : %2. Il n’est demandé qu’une fois ; cette fenêtre se fermera toute seule.")
                                   .arg(pairingName).arg(pin),
                             extra: pinBox, entries: [cancelEntry]}, null, 0, 0)
    }

    function activate(i) {
        var online = role(i, "online"), paired = role(i, "paired")
        if (online && paired) openHost(i)
        else if (online) pairHost(i)
        else if (!role(i, "statusUnknown")) wakeHost(i)
    }

    function pairingComplete(uuid, error) {
        // Another page's pairing, or a pairing of this page that the user left
        if (uuid !== pairingUuid) return
        pairingUuid = ""
        if (dialogKind === "pair" && dialog.opened) dialog.close()
        if (StackView.status !== StackView.Active) return
        if (error !== undefined) {
            showError(error)
            return
        }
        var i = indexOfUuid(uuid)
        var name = i >= 0 ? role(i, "name") : pairingName
        toast.show(qsTr("%1 est appairé").arg(name))
        // Then open it, as Android (pairFinished -> openHost): refused if its LavApollo is too old
        if (i >= 0 && !checkSupported(i)) return
        if (home) {
            home.showHost(uuid)
            stackView.pop()
        }
    }

    // The menu and the dialogs keep the host's uuid, never its row: rows move while they are open
    // (a host comes online, one is added or forgotten), and the row could name another PC
    // The prototype's dialogs (LavOptions): « PC · adresse » head line, title, note, entries. A popover
    // at the pointer for the mouse (right click, ⋯), centred otherwise
    readonly property string sep: Theme.affiche ? " — " : " · "
    function headLine(where) {
        var h = qsTr("PC") + (where ? sep + where : "")
        return Theme.affiche ? h.toUpperCase() : h
    }
    property string dialogKind: ""
    property string dialogUuid: ""
    function showOptions(kind, cfg, item, x, y) {
        dialogKind = kind
        if (item && Theme.input === "mouse") {
            var p = item.mapToItem(null, x, y)
            dialog.showPopover(cfg, p.x, p.y)
        } else {
            dialog.showDialog(cfg)
        }
    }
    readonly property var cancelEntry: ({l: qsTr("Annuler"), a: "cancel"})
    // Type and link of a PC (« Tour · réseau filaire »), from LavApollo HostFormFactor / HostLink
    function typeText(formFactor) {
        return formFactor === "tower" ? qsTr("Tour") : formFactor === "mini" ? qsTr("Mini-PC") : formFactor === "laptop" ? qsTr("Portable") : ""
    }
    function linkText(link) { return link === "wired" ? qsTr("réseau filaire") : link === "wifi" ? qsTr("Wi-Fi") : "" }

    function openHostMenu(item, i, x, y) {
        var online = role(i, "online"), paired = role(i, "paired")
        var list = []
        if (online && paired) {
            list.push({l: qsTr("Ouvrir"), a: "open", main: true})
            list.push({l: qsTr("Benchmark des réglages"), a: "bench"})
        } else if (online) {
            list.push({l: qsTr("Appairer"), a: "pair", main: true})
        } else if (role(i, "wakeable")) {
            list.push({l: qsTr("Réveiller"), a: "wake", main: true})
        }
        list.push({l: qsTr("Renommer"), a: "rename"})
        list.push({l: qsTr("Oublier ce PC"), a: "forget"})
        list.push(cancelEntry)
        if (!list[0].main) list[0].main = true
        dialogUuid = role(i, "uuid")
        var meta = [typeText(role(i, "formFactor")), linkText(role(i, "link"))].filter(function(t) { return t.length > 0 })
        var state = online ? qsTr("En ligne") + (role(i, "apolloVersion") ? " · LavApollo " + role(i, "apolloVersion") : "")
                  : (role(i, "wakeable") ? qsTr("Éteint ou en veille : il peut être réveillé d’ici.") : qsTr("Éteint ou injoignable."))
        showOptions("host", {title: role(i, "name"), head: headLine(role(i, "address")), info: true,
                             note: [state].concat(meta).join(" · "), entries: list}, item, x, y)
    }

    function onDialog(action) {
        var kind = dialogKind
        if (action === "cancel") return
        if (kind === "add") return onAddMenu(action)
        if (kind === "addr") { if (action === "addrOk") addByAddress(dialogField.text); return }
        var i = indexOfUuid(dialogUuid)
        if (i < 0) return
        if (kind === "rename") {
            if (action === "renameOk" && dialogField.text.trim().length > 0) computerModel.renameComputer(i, dialogField.text.trim())
            return
        }
        if (kind === "forget") {
            if (action === "forgetOk") computerModel.deleteComputer(i)
            return
        }
        onHostMenu(action, i)
    }

    function onHostMenu(action, i) {
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
            // as a component: the StackView owns the page and destroys it on pop
            stackView.push(Qt.createComponent("../BenchmarkView.qml"), {"hostUuid": role(i, "uuid"), "hostName": role(i, "name")})
        } else if (action === "rename") {
            dialogUuid = role(i, "uuid")
            dialogField.label = qsTr("Nom")
            dialogField.text = role(i, "name")
            dialogField.placeholder = role(i, "name")
            showOptions("rename", {title: qsTr("Renommer %1").arg(role(i, "name")), head: headLine(role(i, "address")), info: true,
                                   note: qsTr("Le nom de ce PC dans LavArtemis. Le PC lui-même garde le sien."), extra: dialogField,
                                   entries: [{l: qsTr("Renommer"), a: "renameOk", main: true}, cancelEntry]}, null, 0, 0)
        } else if (action === "forget") {
            dialogUuid = role(i, "uuid")
            showOptions("forget", {title: qsTr("Oublier %1 ?").arg(role(i, "name")), head: headLine(role(i, "address")), warn: true,
                                   note: qsTr("Il disparaît de LavArtemis. Pour le retrouver, il faudra l’ajouter et l’appairer à nouveau."),
                                   entries: [{l: qsTr("Oublier"), a: "forgetOk", main: true}, cancelEntry]}, null, 0, 0)
        }
    }

    // « Ajouter un PC » (planche V8-65): the PCs found on the local network, « Saisir une adresse… »
    function openAddMenu(item, x, y) {
        var list = []
        for (var i = 0; i < computerModel.rowCount(); i++) {
            if (!role(i, "paired") && role(i, "online")) {
                var addr = role(i, "address")
                list.push({l: role(i, "name") + (addr ? " · " + addr : ""), a: "pair:" + role(i, "uuid")})
            }
        }
        list.push({l: qsTr("Saisir une adresse…"), a: "manual"})
        list.push(cancelEntry)
        showOptions("add", {title: qsTr("Ajouter un PC"), head: headLine(qsTr("réseau local")), info: true,
                            note: list.length > 2 ? qsTr("PC trouvés sur ton réseau local (Apollo ou Sunshine) :")
                                                  : qsTr("Aucun PC trouvé sur ton réseau local pour l’instant."),
                            entries: list}, null, 0, 0)
    }

    function onAddMenu(action) {
        if (action.indexOf("pair:") === 0) {
            var i = indexOfUuid(action.slice(5))
            if (i >= 0) pairHost(i)
        } else if (action === "manual") {
            dialogField.label = qsTr("Adresse")
            dialogField.text = ""
            dialogField.placeholder = "192.168.1.24"
            showOptions("addr", {title: qsTr("Saisir une adresse"), head: headLine(qsTr("réseau local")), info: true,
                                 note: qsTr("Adresse IP ou nom du PC hôte (par exemple 192.168.1.24)."), extra: dialogField,
                                 entries: [{l: qsTr("Ajouter"), a: "addrOk", main: true}, cancelEntry]}, null, 0, 0)
        }
    }

    function addByAddress(address) {
        address = address.trim()
        if (address.length === 0) return
        // Unreadable: said at once, as Android (v2_add_invalid), instead of « Impossible de joindre »
        if (!ComputerManager.isValidManualAddress(address)) {
            showError(qsTr("« %1 » n'est pas une adresse valide. Saisis une adresse IP ou un nom, avec le port s'il le faut (192.168.1.24:47989).").arg(address))
            return
        }
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
        var adds = pendingAdds
        adds[address] = paired
        pendingAdds = adds
        toast.show(qsTr("Recherche de %1…").arg(address))
        ComputerManager.addNewHostManually(address)
    }

    // uuid = the host that answered at this address, new or already known. ComputerManager emits
    // this after the state change that put a new host in the model, so its row is there.
    // Same outcome as Android's HostsActivity.onAdded(): not paired -> pair it now; paired
    // before -> already in the list; otherwise added.
    function addComplete(success, detectedPortBlocking, uuid, address) {
        if (!(address in pendingAdds)) return
        var pairedBeforeAdd = pendingAdds[address]
        var adds = pendingAdds
        delete adds[address]
        pendingAdds = adds
        if (!success) {
            if (ComputerManager.isWrongSubnetAddress(address)) {
                // Android's v2_add_wrong_subnet: a private address outside this PC's networks
                showError(qsTr("%1 n'est pas sur le réseau de cet appareil. Vérifie l'adresse, et que l'appareil est connecté au même réseau que le PC.").arg(address))
                return
            }
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
        showOptions("error", {title: qsTr("Oups"), head: headLine(""), info: true, note: text,
                              entries: [{l: qsTr("OK"), a: "cancel", main: true}]}, null, 0, 0)
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
                // Prototype V8 hosts: header at 28 px, the cards from 130 px (x k = 35 and 162 on the stage)
                x: 60; y: 22
                width: parent.width - 120
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

                Item { width: 1; height: 24 }

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
                            required property string formFactor
                            required property string link
                            required property int latencyMs

                            // Unpaired hosts are offered by "Ajouter un PC" instead
                            visible: paired
                            width: grid.cardWidth
                            height: 500
                            hoverEnabled: true
                            focusPolicy: Qt.StrongFocus

                            readonly property string status: statusUnknown ? qsTr("Recherche…")
                                : (!online ? qsTr("Hors ligne") : (busy ? qsTr("Session en cours") : qsTr("En ligne")))
                            readonly property string detail: online
                                ? address + (apolloVersion.length > 0 ? " · LavApollo " + apolloVersion : "")
                                : (statusUnknown ? "" : (wakeable ? qsTr("Éteint ou en veille : il peut être réveillé d'ici.")
                                                                  : qsTr("Éteint ou injoignable.")))
                            readonly property string actionLabel: online ? qsTr("Ouvrir") : (statusUnknown || !wakeable ? "" : qsTr("Réveiller"))
                            // « Tour · réseau filaire · 4 ms » (prototype HOSTS: type, net, ms)
                            readonly property string meta: [hostsPage.typeText(formFactor), hostsPage.linkText(link),
                                                            online && latencyMs > 0 ? qsTr("%1 ms").arg(latencyMs) : ""]
                                                           .filter(function(t) { return t.length > 0 }).join(" · ")

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
                                        // The prototype's icon per kind of PC (HOSTS[].icon); the screen when unknown
                                        LavIcon {
                                            anchors.centerIn: parent; size: 38; stroke: 1.8; color: Theme.fg
                                            name: "monitor"
                                            path: card.formFactor === "tower" ? "M7 2h10v20H7zM10 6h4M10 9h4M12 17h.01"
                                                : card.formFactor === "mini" ? "M4 8h16v9H4zM7 12.5h.01M11 12.5h6M6 17v2M18 17v2"
                                                : card.formFactor === "laptop" ? "M5 5h14v10H5zM3 19h18" : ""
                                        }
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
                                    Row {
                                        visible: card.meta.length > 0
                                        spacing: 7
                                        LavIcon {
                                            visible: card.link === "wired" || card.link === "wifi"
                                            anchors.verticalCenter: parent.verticalCenter
                                            size: 17; stroke: 2; color: Theme.muted
                                            path: card.link === "wired" ? "M12 3v6M7 9h10v5H7zM9 14v4M15 14v4M7 21h10"
                                                                        : "M2 9a15 15 0 0 1 20 0M5.5 12.5a10 10 0 0 1 13 0M9 16a5 5 0 0 1 6 0M12 19.5h.01"
                                        }
                                        Text {
                                            text: card.meta
                                            color: Theme.muted
                                            font.family: Theme.textFont
                                            font.pixelSize: 16
                                        }
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

                            FocusRing {
                                active: card.activeFocus && Theme.showFocus
                                radius: Theme.r
                                k: 1.25
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
                        height: 500
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

                        FocusRing {
                            active: addCard.activeFocus && Theme.showFocus
                            radius: Theme.r
                            k: 1.25
                        }
                    }
                }

                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    color: Theme.muted
                    font.family: Theme.textFont
                    font.pixelSize: 17
                    // The prototype's hostsHelp, per input
                    text: Theme.input === "pad" ? qsTr("A : ouvrir le PC. Y ou A maintenu : réveiller, benchmark, renommer, oublier.")
                        : Theme.input === "keys" ? qsTr("Entrée : ouvrir le PC. Touche Menu ou Entrée maintenue : réveiller, benchmark, renommer, oublier.")
                        : Theme.input === "touch" ? qsTr("Touche pour ouvrir le PC. Appui long ou ⋯ : réveiller, benchmark, renommer, oublier.")
                        : qsTr("Clic : ouvrir le PC. Clic droit ou ⋯ : réveiller, benchmark, renommer, oublier.")
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

    LavOptions {
        id: dialog
        onPicked: (a) => hostsPage.onDialog(a)
        onClosed: hostsPage.forceActiveFocus()
    }
    // The pairing code in big digits (prototype ctx.hasPin)
    Rectangle {
        id: pinBox
        property string pin: ""
        visible: false
        implicitHeight: pinText.implicitHeight + 28 * dialog.k
        radius: Theme.rs * dialog.k
        color: Theme.surface2
        Text {
            id: pinText
            anchors.centerIn: parent
            text: pinBox.pin
            color: Theme.fg
            font.family: Theme.displayFont; font.weight: Theme.displayWeight; font.pixelSize: 54 * dialog.k
            font.letterSpacing: 0.2 * 54 * dialog.k
        }
    }
    // A labelled text field (prototype ctx.hasInput): the address, a new name
    Column {
        id: dialogField
        property string label: ""
        property alias text: dialogInput.text
        property string placeholder: ""
        readonly property Item focusTarget: dialogInput.input
        visible: false
        spacing: 6 * dialog.k
        Text { text: dialogField.label; color: Theme.muted; font.family: Theme.textFont; font.weight: Font.Bold; font.pixelSize: 13 * dialog.k }
        LavTextField {
            id: dialogInput
            width: dialogField.width
            placeholderText: dialogField.placeholder
            maximumLength: 64
            onAccepted: dialog.pick(0)
        }
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

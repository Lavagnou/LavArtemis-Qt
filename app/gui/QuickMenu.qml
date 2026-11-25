import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.2
import KeyMacroManager 1.0
import SystemProperties 1.0
import ArtemisSettings 1.0
import "v2"

Rectangle {
    id: quickMenu
    // Fixed size for the menu - small and centered
    width: 500
    height: 400
    // Don't use anchors with SizeViewToRootObject - position manually
    // The theme's colours and fonts (gui/v2/Theme.qml)
    readonly property color fgColor: Theme.fg
    readonly property color mutedColor: Theme.muted
    readonly property color accentColor: Theme.accent

    color: Theme.surface
    radius: Theme.rs
    border.color: Theme.borderColor
    border.width: 1
    visible: true  // Always visible when created
    opacity: 1.0
    
    // Menu state management
    property string currentMenu: "main"  // "main", "server_commands" or "send_keys"
    
    // Toast notification system
    property string toastMessage: ""
    property bool showToast: false

    // Server command data model
    ListModel {        id: serverCommandsModel    }

    // Keyboard shortcuts and macros, rebuilt from KeyMacroManager on open
    ListModel {        id: sendKeysModel    }
    
    // Animation for smooth show/hide
    Behavior on opacity {
        NumberAnimation { duration: 200 }
    }
    
    // Handle keyboard input for navigation
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape) {
            closeMenu()
        } else if (event.key === Qt.Key_Up) {
            menuListView.decrementCurrentIndex()
        } else if (event.key === Qt.Key_Down) {
            menuListView.incrementCurrentIndex()
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            executeCurrentItem()
        }
    }
    
    // Menu content (no longer nested in another rectangle)
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 15

        // Title
        Text {
            text: {
                if (currentMenu === "server_commands") return qsTr("Server Commands")
                if (currentMenu === "send_keys") return qsTr("Send Keys")
                if (currentMenu === "confirm_quit") return qsTr("Are you sure you want to quit?")
                return qsTr("Quick Menu")
            }
            font.pointSize: 24
            font.bold: true
            font.family: Theme.displayFont
            color: quickMenu.accentColor
            Layout.alignment: Qt.AlignHCenter
        }

        // Menu items
        ListView {
            id: menuListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            focus: true
            // The list scrolls in its own box: without this, the rows past its bottom were drawn
            // over the footer button (« Fermer (Échap) » / « ← Retour ») below it
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollIndicator.vertical: ScrollIndicator { }

            model: {
                if (currentMenu === "server_commands") return serverCommandsModel
                if (currentMenu === "send_keys") return sendKeysModel
                if (currentMenu === "confirm_quit") return confirmQuitModel
                return mainMenuModel
            }
            
            delegate: Button {
                width: menuListView.width
                // A row grows for a description on two lines (« Aucune commande de serveur » explains why)
                height: Math.max(60, contentItem.implicitHeight + 20)
                flat: true

                background: Rectangle {
                    color: parent.down || parent.hovered ? Theme.sel : "transparent"
                    border.color: parent.hovered ? quickMenu.accentColor : "transparent"
                    border.width: 2
                    radius: 5
                }

                onClicked: {
                    executeAction(model.action)
                }

                contentItem: RowLayout {
                    anchors.fill: parent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: 15
                    spacing: 15

                    Text {
                        text: model.icon
                        font.pointSize: 20
                        color: quickMenu.fgColor
                        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                        Layout.preferredWidth: 40
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 2

                        Text {
                            text: model.text
                            font.pointSize: 14
                            font.bold: true
                            font.family: Theme.textFont
                            color: quickMenu.fgColor
                        }

                        Text {
                            text: model.description
                            font.pointSize: 10
                            font.family: Theme.textFont
                            color: quickMenu.mutedColor
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }

        // Back/Close button
        Button {
            text: currentMenu === "main" ? qsTr("Close (Esc)") : qsTr("← Back")
            Layout.alignment: Qt.AlignHCenter
            onClicked: {
                if (currentMenu === "main") {
                    closeMenu()
                } else {
                    currentMenu = "main"
                }
            }
        }
    }
    
    // Toast notification overlay
    Rectangle {
        id: toastNotification
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 20
        width: Math.min(parent.width - 40, toastText.implicitWidth + 20)
        height: 40
        radius: 20
        color: "#333"
        border.color: "#666"
        border.width: 1
        visible: showToast
        opacity: showToast ? 1.0 : 0.0
        
        Behavior on opacity {
            NumberAnimation { duration: 200 }
        }
        
        Text {
            id: toastText
            text: toastMessage
            color: "white"
            font.pointSize: 12
            anchors.centerIn: parent
        }
    }
    
    // Timer for auto-hiding toast
    Timer {
        id: toastTimer
        interval: 2000
        onTriggered: {
            showToast = false
        }
    }
    
    // Timer for delayed close
    Timer {
        id: closeTimer
        interval: 1000
        onTriggered: {
            closeMenu()
        }
    }

    // The menu is rebuilt each time it opens and when the host reports a Playnite game: the
    // microphone and touch-mode entries depend on the session's state.
    Connections {
        target: quickMenuManager
        function onPlayniteGameChanged() { rebuildMainMenu() }
        function onVisibilityChanged() {
            if (quickMenuManager.isVisible) {
                currentMenu = "main"
                rebuildMainMenu()
            }
        }
    }

    Connections {
        target: quickMenuManager.serverCommandManager
        // The host's commands, as it listed them (Android: GameMenu.showServerCmd)
        function onCommandsRefreshed() {
            serverCommandsModel.clear();
            var names = quickMenuManager.serverCommandManager.getAvailableCommands();
            for (var i = 0; i < names.length; i++) {
                serverCommandsModel.append({
                    icon: "⚙",
                    text: names[i],
                    // Prefixed: a command may bear the name of one of the menu's own actions
                    action: "server_cmd:" + names[i],
                    description: qsTr("Command of the host PC")
                });
            }
            if (serverCommandsModel.count === 0) {
                // As on Android: say why the list is empty instead of showing nothing
                serverCommandsModel.append({
                    icon: "ℹ",
                    text: qsTr("No server commands"),
                    action: "no_server_command",
                    description: qsTr("Make sure the host runs Apollo / LavApollo and this client has the \"Server Command\" permission.")
                });
            }
        }
        // Said over the stream: the menu closes right after
        function onCommandExecuted(commandName) {
            quickMenuManager.showToast(qsTr("Command \"%1\" sent").arg(commandName))
        }
        function onCommandFailed(commandName, error) {
            quickMenuManager.showToast(qsTr("Command \"%1\" failed: %2").arg(commandName).arg(error))
        }
    }
    
    // Main menu, in the order shared with Android's in-game menu (common entries first, then what
    // only this PC has). Rebuilt on open: see rebuildMainMenu().
    ListModel {
        id: mainMenuModel
    }

    // Confirmation of « Quitter la session », with the texts of Android's confirmation
    ListModel {
        id: confirmQuitModel
        ListElement {
            text: qsTr("Quit Session")
            icon: "❌"
            action: "quit"
            description: qsTr("Make sure your progress is saved: quitting the session closes the app running on the PC.")
        }
    }

    function rebuildMainMenu() {
        mainMenuModel.clear()
        if (quickMenuManager.playniteGameName !== "") {
            // LavApollo Link: a game runs inside the host's Playnite -> "Quitter <jeu>", first entry
            mainMenuModel.append({icon: "⏏", text: qsTr("Quitter %1").arg(quickMenuManager.playniteGameName),
                                  action: "quit_playnite_game", description: qsTr("Retour à Playnite, le stream continue")})
        }
        mainMenuModel.append({icon: "⏹", text: qsTr("Disconnect"), action: "disconnect",
                              description: qsTr("Disconnect from server")})
        mainMenuModel.append({icon: "❌", text: qsTr("Quit Session"), action: "confirm_quit",
                              description: qsTr("Quit streaming session")})
        mainMenuModel.append({icon: "⚙", text: qsTr("Server Commands"), action: "server_commands",
                              description: qsTr("Access server control commands")})
        mainMenuModel.append({icon: "⌨", text: qsTr("Send Keys"), action: "send_keys",
                              description: qsTr("Send a shortcut the local OS would otherwise intercept")})
        mainMenuModel.append({icon: "📋", text: qsTr("Clipboard Upload"), action: "clipboard_upload",
                              description: qsTr("Upload clipboard to server")})
        mainMenuModel.append({icon: "📥", text: qsTr("Fetch Clipboard"), action: "clipboard_fetch",
                              description: qsTr("Fetch clipboard from server")})
        mainMenuModel.append({icon: "📊", text: qsTr("Toggle Performance Stats"), action: "toggle_stats",
                              description: qsTr("Show/hide performance statistics")})
        // Only while the microphone is sent, with the label of what it will do (as on Android)
        if (quickMenuManager.microphoneActive()) {
            mainMenuModel.append({icon: "🎤",
                                  text: quickMenuManager.microphoneMuted() ? qsTr("Unmute Microphone") : qsTr("Mute Microphone"),
                                  action: "toggle_microphone",
                                  description: qsTr("Stop or resume sending your microphone (Ctrl+Alt+Shift+U)")})
        }
        // Touchscreen PCs: switch Multitouch <-> Trackpad for this stream (Android: Menu avancé)
        if (SystemProperties.hasTouchScreen) {
            var trackpad = quickMenuManager.trackpadTouchMode()
            mainMenuModel.append({icon: "👆",
                                  text: trackpad ? qsTr("Switch to Multitouch") : qsTr("Switch to Trackpad"),
                                  action: "toggle_touch_mode",
                                  description: quickMenuManager.rememberTouchMode() ? qsTr("Kept for the next streams")
                                                                                     : qsTr("For this stream only")})
        }
        // This PC only: the actions of the Ctrl+Alt+Shift shortcuts Z, K and X
        mainMenuModel.append({icon: "🖱", text: qsTr("Release Mouse and Keyboard"), action: "toggle_mouse",
                              description: qsTr("Gives the pointer back to your PC.")})
        mainMenuModel.append({icon: "⌨", text: qsTr("Capture Keyboard"), action: "toggle_keyboard",
                              description: qsTr("Sends the system shortcuts to the host PC, or not.")})
        mainMenuModel.append({icon: "🖥", text: qsTr("Full Screen / Window"), action: "toggle_fullscreen",
                              description: qsTr("Switches how the stream is displayed.")})
    }

    function closeMenuDelayed() {
        closeTimer.restart();
    }

    // Functions
    function closeMenu() {
        // Only call backend hide - don't set QML invisible
            if (typeof quickMenuManager !== 'undefined') {
            quickMenuManager.hide();
        }
    }
    
    function populateSendKeys() {
        sendKeysModel.clear()

        var macros = KeyMacroManager.macros
        for (var i = 0; i < macros.length; i++) {
            sendKeysModel.append({
                icon: macros[i].builtIn ? "⌨" : "★",
                text: macros[i].name,
                // Prefixed so executeAction() can tell these apart from the
                // server command ids it also matches against.
                action: "macro:" + macros[i].id,
                description: macros[i].builtIn ? qsTr("Built-in shortcut")
                                               : qsTr("Custom macro")
            })
        }
    }

    function executeCurrentItem() {
        var currentItem = menuListView.model.get(menuListView.currentIndex)
        if (currentItem) {
            executeAction(currentItem.action)
        }
    }
    
    function executeAction(action) {
        console.log("Executing action:", action)
        
        // Handle navigation actions
        if (action === "confirm_quit") {
            currentMenu = "confirm_quit";
            return;
        } else if (action === "no_server_command") {
            return;
        } else if (action === "server_commands") {
            // Always open, as on Android: an empty list says why
            currentMenu = "server_commands";
            return;
        } else if (action === "quit_playnite_game") {
            quickMenuManager.quitPlayniteGame();
            closeMenuDelayed();
            return;
        } else if (action === "send_keys") {
            populateSendKeys();
            currentMenu = "send_keys";
            return;
        } else if (action.indexOf("macro:") === 0) {
            KeyMacroManager.sendMacro(action.substring(6));
            closeMenuDelayed();
            return;
        } else if (action.indexOf("server_cmd:") === 0) {
            quickMenuManager.serverCommandManager.executeCommand(action.substring(11));
            closeMenuDelayed();
            return;
        }
        
        // Show action feedback
        showActionFeedback(action)
        
        // Call the backend manager to execute the action
        if (typeof quickMenuManager !== 'undefined') {
            quickMenuManager.executeAction(action)
        }
        
        // Close menu after action (except for navigation)
        closeMenuDelayed();
    }
    
    function showActionFeedback(action) {
        var message = ""
        switch(action) {
            case "disconnect":
                message = qsTr("Disconnecting from server...")
                break
            case "quit":
                message = qsTr("Quitting session...")
                break
            case "clipboard_upload":
                message = qsTr("Uploading clipboard to server...")
                break
            case "clipboard_fetch":
                message = qsTr("Fetching clipboard from server...")
                break
            case "toggle_stats":
                message = qsTr("Toggling performance stats...")
                break
            case "toggle_mouse":
                message = qsTr("Toggling mouse capture...")
                break
            case "toggle_keyboard":
                message = qsTr("Toggling keyboard capture...")
                break
            case "toggle_fullscreen":
                message = qsTr("Toggling fullscreen...")
                break
            case "toggle_microphone":
                message = qsTr("Toggling microphone...")
                break
            case "toggle_touch_mode":
                // The mode it switches to (the menu was built with the old one)
                message = quickMenuManager.trackpadTouchMode()
                        ? qsTr("Multitouch mode")
                        : qsTr("Trackpad mode: long press to drag, two-finger tap for right click, two fingers to scroll")
                break
            default:
                message = qsTr("Executing action...")
        }
        
        showMessage(message)
    }

    function showMessage(message) {
        if (message) {
            toastMessage = message
            showToast = true
            toastTimer.restart()
        }
    }
    
    // Make menu focusable and reset state
    Component.onCompleted: {
        focus = true
        currentMenu = "main"  // Always start with main menu
        rebuildMainMenu()

        if (quickMenuManager.serverCommandManager) {
            console.log("QuickMenu: Refreshing server commands on open");
            quickMenuManager.serverCommandManager.refreshCommands();
        }
    }
}


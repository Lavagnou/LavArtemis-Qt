import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.2
import ServerCommandManager 1.0
import KeyMacroManager 1.0

Rectangle {
    id: quickMenu
    // Fixed size for the menu - small and centered
    width: 500
    height: 400
    // Don't use anchors with SizeViewToRootObject - position manually
    color: "#2d2d2d"
    radius: 10
    border.color: "#444"
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
                return qsTr("Quick Menu")
            }
            font.pointSize: 24
            font.bold: true
            color: "#00cccc"
            Layout.alignment: Qt.AlignHCenter
        }

        // Menu items
        ListView {
            id: menuListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            focus: true
            
            model: {
                if (currentMenu === "server_commands") return serverCommandsModel
                if (currentMenu === "send_keys") return sendKeysModel
                return mainMenuModel
            }
            
            delegate: Button {
                width: menuListView.width
                height: 60
                flat: true

                background: Rectangle {
                    color: parent.down ? "#333" : (parent.hovered ? "#444" : "transparent")
                    border.color: parent.hovered ? "#00cccc" : "transparent"
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
                        color: "white"
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
                            color: "white"
                        }

                        Text {
                            text: model.description
                            font.pointSize: 10
                            color: "#cccccc"
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

    // ServerCommandManager connection
    Connections {
        target: quickMenuManager.serverCommandManager
        function onCommandsRefreshed() {
            console.log("Server commands refreshed, updating model...");
            serverCommandsModel.clear();
            var commandIds = quickMenuManager.serverCommandManager.getAvailableCommands();
            for (var i = 0; i < commandIds.length; i++) {
                var commandId = commandIds[i];
                var icon = "⚙️"; // Default
                if (commandId.toLowerCase() === "shutdown") {
                    icon = "⏻";
                } else if (commandId.toLowerCase() === "restart") {
                    icon = "🔄";
                }
                serverCommandsModel.append({
                    icon: icon,
                    text: quickMenuManager.serverCommandManager.getCommandName(commandId),
                    action: commandId, // Use command ID as the action
                    description: quickMenuManager.serverCommandManager.getCommandDescription(commandId)
                });
            }
            console.log("Server commands model updated with", serverCommandsModel.count, "commands");
        }
        function onCommandExecuted(commandId, success, result) {
            showActionFeedback(success ? "Command '" + commandId + "' executed successfully" : "Command '" + commandId + "' failed: " + result);
        }
    }
    
    // Main menu model
    ListModel {
        id: mainMenuModel
        ListElement {
            text: qsTr("Disconnect")
            icon: "⏹"
            action: "disconnect"
            description: qsTr("Disconnect from server")
        }
        ListElement {
            text: qsTr("Quit")
            icon: "❌"
            action: "quit"
            description: qsTr("Quit streaming session")
        }
        ListElement {
            text: qsTr("Server Commands")
            icon: "⚙"
            action: "server_commands"
            description: qsTr("Access server control commands")
        }
        ListElement {
            text: qsTr("Send Keys")
            icon: "⌨"
            action: "send_keys"
            description: qsTr("Send a shortcut the local OS would otherwise intercept")
        }
        ListElement {
            text: qsTr("Clipboard Upload")
            icon: "📋"
            action: "clipboard_upload"
            description: qsTr("Upload clipboard to server")
        }
        ListElement {
            text: qsTr("Fetch Clipboard")
            icon: "📥"
            action: "clipboard_fetch"
            description: qsTr("Fetch clipboard from server")
        }
        ListElement {
            text: qsTr("Toggle Performance Stats")
            icon: "📊"
            action: "toggle_stats"
            description: qsTr("Show/hide performance statistics")
        }
        ListElement {
            text: qsTr("Toggle Mouse Capture")
            icon: "🖱"
            action: "toggle_mouse"
            description: qsTr("Toggle mouse capture mode")
        }
        ListElement {
            text: qsTr("Toggle Keyboard Capture")
            icon: "⌨"
            action: "toggle_keyboard"
            description: qsTr("Toggle keyboard capture mode")
        }
        ListElement {
            text: qsTr("Toggle Fullscreen")
            icon: "🖥"
            action: "toggle_fullscreen"
            description: qsTr("Toggle fullscreen mode")
        }
    }
    
    function closeMenuDelayed() {
        closeTimer.restart();
    }

    // Functions
    function closeMenu() {
        // Only call backend hide - don't set QML invisible
            if (typeof quickMenuManager !== 'undefined') {
                showActionFeedback("Closing menu...")  // Feedback when closing
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
        if (action === "server_commands") {
            if (quickMenuManager.serverCommandManager && quickMenuManager.serverCommandManager.hasPermission) {
                currentMenu = "server_commands";
            } else {
                showActionFeedback("Server commands not available or no permission");
                closeMenuDelayed();
            }
            return;
        } else if (action === "send_keys") {
            populateSendKeys();
            currentMenu = "send_keys";
            return;
        } else if (action.indexOf("macro:") === 0) {
            KeyMacroManager.sendMacro(action.substring(6));
            closeMenuDelayed();
            return;
        } else { // It might be a server command
            for (var i = 0; i < serverCommandsModel.count; i++) {
                if (serverCommandsModel.get(i).action === action) {
                    if (quickMenuManager.serverCommandManager) {
                        quickMenuManager.serverCommandManager.executeCommand(action);
                        closeMenuDelayed();
                    }
                    return;
                }
            }
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
                message = "Disconnecting from server..."
                break
            case "quit":
                message = "Quitting session..."
                break
            case "server_restart":
                message = ServerCommandManager.hasServerCommands ? "Restarting server..." : "Server commands not available"
                break
            case "server_shutdown":
                message = ServerCommandManager.hasServerCommands ? "Shutting down server..." : "Server commands not available"
                break
            case "server_suspend":
                message = ServerCommandManager.hasServerCommands ? "Suspending server..." : "Server commands not available"
                break
            case "clipboard_upload":
                message = "Uploading clipboard to server..."
                break
            case "clipboard_fetch":
                message = "Fetching clipboard from server..."
                break
            case "toggle_stats":
                message = "Toggling performance stats..."
                break
            case "toggle_mouse":
                message = "Toggling mouse capture..."
                break
            case "toggle_keyboard":
                message = "Toggling keyboard capture..."
                break
            case "toggle_fullscreen":
                message = "Toggling fullscreen..."
                break
            default:
                message = "Executing action..."
        }
        
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

        if (quickMenuManager.serverCommandManager) {
            console.log("QuickMenu: Refreshing server commands on open");
            quickMenuManager.serverCommandManager.refreshCommands();
        }
    }
}


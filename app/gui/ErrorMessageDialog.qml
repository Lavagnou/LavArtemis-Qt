import QtQuick 2.0
import QtQuick.Controls 2.2

// An error with a single OK. No Help button: it opened Moonlight's troubleshooting wiki (in
// English), and the user wants no help button anywhere (same as the Android client).
NavigableMessageDialog {
    standardButtons: Dialog.Ok
}

import QtQuick
import QtQuick.Controls

MenuBar {
    id: menuBar
    width: parent.width

    signal fileInfoTriggered()
    signal logsApplicationTriggered(bool checked)
    signal logsBridgeSyncTriggered(bool checked)
    signal logsAppHostTriggered(bool checked)
    signal settingsEngineTriggered()
    signal settingsApplicationTriggered()
    signal contentBrowseToggled(bool checked)  // Use a parameter for checkable actions
    signal helpShortcutsTriggered()

    property alias logsApplicationChecked: logsApplicationAction.checked
    property alias logsBridgeSyncChecked: logsBridgeSyncAction.checked
    property alias logsAppHostChecked: logsAppHostAction.checked
    property alias contentBrowseChecked: contentBrowseAction.checked

    Menu {
        title: "File"
        width: 200
        // Action {
        //     text: "Info"
        //     shortcut: "CTRL+I"
        //     onTriggered: menuBar.fileInfoTriggered()
        // }
        // MenuSeparator {}
        Action {
            text: "Quit"
            shortcut: "CTRL+Q"
            onTriggered: Qt.quit()
        }
    }
    Menu {
        title: "Logs"
        width: 250
        Action {
            id: logsApplicationAction
            text: "Application"
            shortcut: "C" // "CTRL+L,CTRL+A"
            checkable: true
            onTriggered: menuBar.logsApplicationTriggered(checked)
        }
        Action {
            id: logsBridgeSyncAction
            text: "BridgeSync"
            shortcut: "SHIFT+/" // "CTRL+L,CTRL+B"
            checkable: true
            onTriggered: menuBar.logsBridgeSyncTriggered(checked)
        }
        Action {
            id: logsAppHostAction
            text: "AppHost"
            shortcut: "CTRL+L,CTRL+H"
            checkable: true
            onTriggered: menuBar.logsAppHostTriggered(checked)
        }
    }
    Menu {
        title: "Settings"
        width: 200
        Action {
            text: "Engine"
            onTriggered: menuBar.settingsEngineTriggered()
        }
        Action {
            text: "Application"
            onTriggered: menuBar.settingsApplicationTriggered()
        }
    }
    Menu {
        title: "Content"
        width: 200
        Action {
            id: contentBrowseAction
            text: "Browse"
            checkable: true
            onTriggered: menuBar.contentBrowseToggled(checked)
        }
    }
    Menu {
        title: "Help"
        width: 200
        Action {
            text: "Shortcuts"
            onTriggered: menuBar.helpShortcutsTriggered()
        }
    }
}

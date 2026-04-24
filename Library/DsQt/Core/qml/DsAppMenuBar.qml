import QtQuick
import QtQuick.Controls

MenuBar {
    id: menuBar


    signal fileInfoTriggered()
    signal logsApplicationTriggered(bool checked)
    signal logsBridgeSyncTriggered(bool checked)
    signal logsAppHostTriggered(bool checked)
    signal settingsEngineTriggered()
    signal settingsApplicationTriggered()
    signal contentBrowseToggled(bool checked)  // Use a parameter for checkable actions
    signal helpShortcutsTriggered()
    signal touchFilterDebugTriggered(bool checked)

    property alias logsApplicationChecked: logsApplicationAction.checked
    property alias logsBridgeSyncChecked: logsBridgeSyncAction.checked
    property alias logsAppHostChecked: logsAppHostAction.checked
    property alias contentBrowseChecked: contentBrowseAction.checked
    property alias touchFilterDebugChecked: touchFilterDebugAction.checked

    Menu {
        id: fileMenu
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
        id: logsMenu
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
        id: toolsMenu
        title: "Tools"
        width: 200
        Action {
            checkable: true
            text: "Settings"
            onTriggered: menuBar.settingsEngineTriggered()
        }

        Action {
            id: contentBrowseAction
            text: "Content Browser"
            checkable: true
            onTriggered: menuBar.contentBrowseToggled(checked)
        }

        Action {
            id: touchFilterDebugAction
            text: "TouchFilter Debug"
            checkable: true
            onTriggered: menuBar.touchFilterDebugTriggered(checked)
        }
    }
    Menu {
        id: helpMenu
        title: "Help"
        width: 200
        Action {
            text: "Shortcuts"
            onTriggered: menuBar.helpShortcutsTriggered()
        }
    }
}

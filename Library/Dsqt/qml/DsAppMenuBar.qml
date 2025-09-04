import QtQuick
import QtQuick.Controls
import Dsqt

MenuBar {
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
        Action {
            text: "Info"
            shortcut: "CTRL+I"
            onTriggered: fileInfoTriggered()
        }
        MenuSeparator {}
        Action {
            text: "Quit"
            shortcut: "CTRL+Q"
            onTriggered: Qt.quit()
        }

        //delegate: DsAppMenuItem {}
    }
    Menu {
        title: "Logs"
        width: 250
        Action {
            id: logsApplicationAction
            text: "Application"
            shortcut: "CTRL+L,CTRL+A"
            checkable: true
            onTriggered: logsApplicationTriggered(checked)
        }
        Action {
            id: logsBridgeSyncAction
            text: "BridgeSync"
            shortcut: "CTRL+L,CTRL+B"
            checkable: true
            onTriggered: logsBridgeSyncTriggered(checked)
        }
        Action {
            id: logsAppHostAction
            text: "AppHost"
            shortcut: "CTRL+L,CTRL+H"
            checkable: true
            onTriggered: logsAppHostTriggered(checked)
        }

        //delegate: DsAppMenuItem {}
    }
    Menu {
        title: "Settings"
        width: 200
        Action {
            text: "Engine"
            onTriggered: settingsEngineTriggered()
        }
        Action {
            text: "Application"
            onTriggered: settingsApplicationTriggered()
        }

        //delegate: DsAppMenuItem {}
    }
    Menu {
        title: "Content"
        width: 200
        Action {
            id: contentBrowseAction
            text: "Browse"
            checkable: true
            onTriggered: contentBrowseToggled(checked)
        }

        //delegate: DsAppMenuItem {}
    }
    Menu {
        title: "Help"
        width: 200
        Action {
            text: "Shortcuts"
            onTriggered: helpShortcutsTriggered()
        }

        //delegate: DsAppMenuItem {}
    }
}

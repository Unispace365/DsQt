import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import ProjectCloner

Window {
    id: root
    width: Math.min(980, Screen.desktopAvailableWidth - 48)
    minimumWidth: 720
    height: Math.min(900, Screen.desktopAvailableHeight - 64)
    minimumHeight: 480
    visible: true
    title: qsTr("Project Cloner")

    Loader {
        id: loader
        anchors.fill: parent
        source: "./MainView.qml"
    }
    Shortcut {
        sequence: "CTRL+R"
        onActivated: {
            let src = loader.source;
            loader.source = "";
            loader.source = src+"?tm="+Date.now();
            console.log(loader.source)
        }
    }
}

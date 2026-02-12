import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import ProjectCloner

Window {
    id: root
    width: 640
    height: 480
    visible: true
    title: qsTr("Project Cloner")

    Loader {
        id: loader
        anchors.fill: parent
        source: "file:///C:/dev/DsQt/Tools/ProjectCloner/MainView.qml"
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

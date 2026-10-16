import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import ProjectCloner

Pane {
    id: root
    padding: 24
    ColumnLayout {
        anchors.fill: parent
        spacing: 16
        Label { text: qsTr("Create a DsQt application"); font.pixelSize: 25; font.bold: true }
        Label { text: qsTr("Choose your package, modules and Qt kit before creating the project."); Layout.fillWidth: true; wrapMode: Text.WordWrap }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                enabled: projectCloner.status !== Cloner.CLONING
                width: parent.width
                spacing: 18
                GroupBox {
                    title: qsTr("Project")
                    Layout.fillWidth: true
                    GridLayout {
                        anchors.fill: parent
                        columns: 3
                        Label { text: qsTr("Project folder") }
                        TextField { id: projectRoot; Layout.fillWidth: true; text: settings.projectRootDir; onEditingFinished: settings.projectRootDir = text }
                        Button { text: qsTr("Browse…"); onClicked: projectFolder.open() }
                        Label { text: qsTr("Project name") }
                        TextField { id: projectName; Layout.fillWidth: true; Layout.columnSpan: 2; placeholderText: qsTr("MyApplication"); validator: RegularExpressionValidator { regularExpression: /[A-Za-z][0-9A-Za-z_ ]*/ } maximumLength: 40 }
                        CheckBox { id: platformToggle; text: qsTr("Use a platform subdirectory"); Layout.columnSpan: 3; checked: false }
                        Label { text: qsTr("Platform name"); visible: platformToggle.checked }
                        TextField { id: platformName; visible: platformToggle.checked; Layout.fillWidth: true; Layout.columnSpan: 2; validator: RegularExpressionValidator { regularExpression: /[A-Za-z][0-9A-Za-z_ ]*/ } }
                        CheckBox { id: topLevel; text: qsTr("Include project-level files"); checked: true; Layout.columnSpan: 3 }
                    }
                }
                PackageSelection { Layout.fillWidth: true; cloner: projectCloner }
            }
        }
        Label {
            Layout.fillWidth: true
            text: projectCloner.lastError || (projectCloner.status === Cloner.POSTCLONE ? qsTr("Created %1").arg(projectCloner.cloneToDirectory) : qsTr("Destination: %1").arg(projectCloner.cloneToDirectory))
            wrapMode: Text.WrapAnywhere
        }
        RowLayout {
            Layout.fillWidth: true
            Button { text: qsTr("Show console"); onClicked: consoleWindow.show() }
            Item { Layout.fillWidth: true }
            Button { text: qsTr("Reset"); visible: projectCloner.status === Cloner.POSTCLONE; onClicked: projectCloner.reset() }
            Button {
                text: projectCloner.status === Cloner.CLONING ? qsTr("Creating…") : qsTr("Create project")
                highlighted: true
                enabled: projectCloner.status === Cloner.PRECLONE && projectCloner.canClone && projectRoot.text.length > 0 && projectName.acceptableInput && (!platformToggle.checked || platformName.acceptableInput)
                onClicked: projectCloner.clone()
            }
        }
    }
    Cloner {
        id: projectCloner
        readonly property string folderName: projectName.text.replace(/ /g, "")
        applicationName: platformToggle.checked ? platformName.text.replace(/ /g, "") : folderName
        projectName: projectName.text
        platformName: platformToggle.checked ? platformName.text : projectName.text
        cloneToDirectory: projectRoot.text + "/" + folderName + (platformToggle.checked ? "/" + applicationName : "")
        copyTopLevel: topLevel.checked
        usePlatformDir: platformToggle.checked
    }
    Settings { id: settings; property string projectRootDir: "C:/dev" }
    FolderDialog {
        id: projectFolder
        onAccepted: {
            projectRoot.text = decodeURIComponent(selectedFolder.toString().replace(/^file:\/\//, "").replace(/^\/(?=[A-Za-z]:)/, ""));
            settings.projectRootDir = projectRoot.text;
        }
    }
    Window { id: consoleWindow; title: qsTr("ProjectCloner console"); width: 760; height: 480; visible: false; ConsoleOutputView { anchors.fill: parent } }
}

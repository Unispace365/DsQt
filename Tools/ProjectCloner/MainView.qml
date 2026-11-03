import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import ProjectCloner

Pane {
    id: root
    padding: 24
    readonly property string projectFolderPath: projectCloner.localFolderPath(projectRoot.text)

    function rememberProjectFolder(normalizeText: bool): void {
        const folder = projectCloner.localFolderPath(projectRoot.text);
        if (!folder)
            return;
        settings.setValue("projectRootDir", folder);
        settings.sync();
        if (normalizeText)
            projectRoot.text = folder;
    }

    Component.onCompleted: {
        const saved = settings.value("projectRootDir", "").toString();
        projectRoot.text = projectCloner.localFolderPath(saved) || projectCloner.defaultProjectDirectory;
        root.rememberProjectFolder(true);
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: projectCloner.status === Cloner.POSTCLONE ? 1 : 0
        ColumnLayout {
            objectName: "projectForm"
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
                            TextField {
                                id: projectRoot
                                objectName: "projectFolderField"
                                Layout.fillWidth: true
                                Accessible.name: qsTr("Project folder")
                                onTextEdited: root.rememberProjectFolder(false)
                                onEditingFinished: root.rememberProjectFolder(true)
                            }
                            Button {
                                objectName: "browseProjectFolderButton"
                                text: qsTr("Browse…")
                                onClicked: {
                                    projectFolder.currentFolder = projectCloner.folderUrl(root.projectFolderPath || projectCloner.defaultProjectDirectory);
                                    projectFolder.open();
                                }
                            }
                            Label { text: qsTr("Project name") }
                            TextField { id: projectName; objectName: "projectNameField"; Layout.fillWidth: true; Layout.columnSpan: 2; placeholderText: qsTr("MyApplication"); validator: RegularExpressionValidator { regularExpression: /[A-Za-z][0-9A-Za-z_ ]*/ } maximumLength: 40 }
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
                text: projectCloner.lastError || qsTr("Destination: %1").arg(projectCloner.cloneToDirectory)
                wrapMode: Text.WrapAnywhere
            }
            RowLayout {
                Layout.fillWidth: true
                Button { text: qsTr("Show console"); onClicked: consoleWindow.show() }
                Item { Layout.fillWidth: true }
                Button {
                    objectName: "createProjectButton"
                    text: projectCloner.status === Cloner.CLONING ? qsTr("Creating…") : qsTr("Create project")
                    highlighted: true
                    enabled: projectCloner.status === Cloner.PRECLONE && projectCloner.canClone && root.projectFolderPath.length > 0 && projectName.acceptableInput && (!platformToggle.checked || platformName.acceptableInput)
                    onClicked: {
                        root.rememberProjectFolder(true);
                        resultView.animationIndex = Math.floor(Math.random() * resultView.animations.length);
                        projectCloner.clone();
                    }
                }
            }
        }
        CloneResultView {
            id: resultView
            objectName: "cloneResultView"
            destination: projectCloner.cloneToDirectory
            errorMessage: projectCloner.lastError
            onCloneAnother: projectCloner.reset()
            onQuitRequested: Qt.quit()
        }
    }
    Cloner {
        id: projectCloner
        objectName: "projectCloner"
        readonly property string folderName: projectName.text.replace(/ /g, "")
        applicationName: platformToggle.checked ? platformName.text.replace(/ /g, "") : folderName
        projectName: projectName.text
        platformName: platformToggle.checked ? platformName.text : projectName.text
        cloneToDirectory: root.projectFolderPath ? localFolderPath(root.projectFolderPath + "/" + folderName + (platformToggle.checked ? "/" + applicationName : "")) : ""
        copyTopLevel: topLevel.checked
        usePlatformDir: platformToggle.checked
    }
    Settings { id: settings }
    FolderDialog {
        id: projectFolder
        objectName: "projectFolderDialog"
        title: qsTr("Choose the project folder")
        onAccepted: {
            projectRoot.text = projectCloner.localFolderPath(selectedFolder.toString());
            root.rememberProjectFolder(true);
        }
    }
    Window { id: consoleWindow; title: qsTr("ProjectCloner console"); width: 760; height: 480; visible: false; ConsoleOutputView { anchors.fill: parent } }
}

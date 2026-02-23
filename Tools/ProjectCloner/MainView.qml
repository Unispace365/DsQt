import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import ProjectCloner

Rectangle {
    id: root
    visible: true


    Window {
        id:consoleOutputWindow
        title: "Console Output"
        width: 600
        height: 400
        visible: false
        ConsoleOutputView {
            id: consoleOutputView
            anchors.fill: parent
            anchors.margins: 5
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Item {
        id: toplevel
        width: root.width
        height: root.height
        property int labelWidth: 150

        ColumnLayout {
            anchors.fill: toplevel
            anchors.margins: 10
            spacing: 10
            Text {
                Layout.alignment: Qt.AlignTop
                id: title
                text: "DsQt ProjectCloner"
            }
            Rectangle {
                Layout.alignment: Qt.AlignTop
                Layout.preferredHeight: childrenRect.height
                Layout.fillWidth: true
                color: "#f0f0f0"
                RowLayout {

                    Text {
                        Layout.preferredWidth: toplevel.labelWidth
                        text: "Project Directory:"
                    }

                    TextField {
                        id: projectRoot
                        Layout.minimumWidth: 300
                        placeholderText: "Projects Root Directory"
                        text: projectRootChooser.currentFolder.toString().replace(/^file:\/\/\//,"");

                    }
                    Button {
                        text: "Pick Folder"
                        onPressed: {
                            projectRootChooser.open();
                        }
                    }
                }
            }
            Rectangle {
                Layout.preferredHeight: childrenRect.height
                Layout.fillWidth: true
                color: "#f0f0f0"
                RowLayout {
                    Text {
                        Layout.preferredWidth: toplevel.labelWidth
                        text: "Project Name:"
                    }
                    TextField {
                        id: projectName
                        Layout.alignment: Qt.AlignTop
                        Layout.minimumWidth: 300
                        validator: RegularExpressionValidator {
                            regularExpression: /[0-9A-Za-z\_ ]+/;

                        }
                        maximumLength: 20
                        placeholderText: "Project Name"
                    }
                }
            }
            Rectangle {
                visible: usePlatformDirCheckBox.checked
                Layout.preferredHeight: visible ? childrenRect.height : 0
                Layout.fillWidth: true
                color: "#f0f0f0"
                RowLayout {
                    Text {
                        Layout.preferredWidth: toplevel.labelWidth
                        text: "Platform Name:"
                    }
                    TextField {
                        id: platformName
                        Layout.alignment: Qt.AlignTop
                        Layout.minimumWidth: 300
                        validator: RegularExpressionValidator {
                            regularExpression: /[0-9A-Za-z\_ ]+/;

                        }
                        maximumLength: 20
                        placeholderText: "Platform Name"
                    }
                }
            }
            RowLayout {
                CheckBox {
                    id: copyTopLevelCheckBox
                    text: "Copy top-level project files"
                    checked: true
                }
            }
            RowLayout {
                CheckBox {
                    id: usePlatformDirCheckBox
                    text: "Use platform subdirectory"
                    checked: true
                }
            }
            Rectangle {
                Layout.preferredHeight: childrenRect.height
                Layout.fillWidth: true
                color: "#f0f0f0"
                ColumnLayout {
                    width: parent.width
                    Text {
                        Layout.preferredWidth: toplevel.labelWidth
                        text: "Qt Versions:"
                    }
                    ListView {
                        id: qtVersionList
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(contentHeight, 120)
                        clip: true
                        model: cloner.detectedQtVersions
                        delegate: CheckBox {
                            text: "Qt " + modelData
                            checked: cloner.selectedQtVersions.indexOf(modelData) >= 0
                            onToggled: {
                                var selected = [...cloner.selectedQtVersions];
                                if (checked) {
                                    if (selected.indexOf(modelData) < 0) selected.push(modelData);
                                } else {
                                    var idx = selected.indexOf(modelData);
                                    if (idx >= 0) selected.splice(idx, 1);
                                }
                                cloner.selectedQtVersions = selected;
                            }
                        }
                    }
                    Text {
                        visible: cloner.detectedQtVersions.length === 0
                        text: "No Qt installations found in C:/Qt/ or in %QT_DIR%"
                        color: "#999"
                        font.italic: true
                    }
                }
            }
            RowLayout {
                Text {
                    Layout.preferredWidth: toplevel.labelWidth
                    text: "Main Name:"
                }
                Text {
                    id: appName
                    Layout.alignment: Qt.AlignTop
                    Layout.minimumWidth: 300
                    text: usePlatformDirCheckBox.checked
                          ? platformName.text.replace(/\ /g,"")
                          : projectName.text.replace(/\ /g,"")
                }
            }
            RowLayout {
                Text {
                    Layout.preferredWidth: toplevel.labelWidth
                    text: "Application Filename:"
                }
                Text {
                    id: appFileName
                    Layout.alignment: Qt.AlignTop
                    Layout.minimumWidth: 300
                    property string inName: usePlatformDirCheckBox.checked
                                             ? platformName.text.replace(/\ /g,"")
                                             : projectName.text.replace(/\ /g,"")
                    text: inName.length>0?"app"+inName.charAt(0).toUpperCase() + inName.slice(1):""
                }
            }
            RowLayout {
                Text {
                    Layout.preferredWidth: toplevel.labelWidth
                    text: "Full Path:"
                }
                Text {
                    id: className
                    Layout.alignment: Qt.AlignTop
                    Layout.minimumWidth: 300
                    text: usePlatformDirCheckBox.checked
                          ? projectRoot.text+"/" + (projectName.text===""?"<project>":projectName.text) + "/" + (appName.text===""?"<platform>":appName.text)
                          : projectRoot.text+"/" + (projectName.text===""?"<project>":projectName.text)
                }
            }
            ColumnLayout {
                Layout.preferredWidth: childrenRect.width
                Layout.topMargin: 20
                Layout.alignment: Qt.AlignHCenter
                Layout.fillHeight: true
                Layout.verticalStretchFactor: 1
                AnimatedImage {
                    id: successImage
                    property list<url> images: ["images/clone1.gif","images/clone2.gif","images/clone3.gif","images/clone4.gif"]
                    property int index: 0
                    source: cloner.lastError=="" ? images[index] : "images/error.gif"
                    opacity: cloner.status==Cloner.POSTCLONE ? 1 : 0
                    fillMode: Image.PreserveAspectFit
                    Layout.preferredWidth: 200
                    Layout.preferredHeight: 100
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
                    Behavior on opacity {
                        NumberAnimation {
                            duration: 200
                            easing.type: Easing.Linear
                        }
                    }
                }
                Text {
                    id: successText
                    text: cloner.lastError=="" ? "Project cloned to "+projectRoot.text+"/"+appName.text : cloner.lastError
                    opacity: cloner.status==Cloner.POSTCLONE ? 1 : 0
                    Layout.alignment: Qt.AlignHCenter
                    Layout.maximumWidth: toplevel.width * 0.8
                    Behavior on opacity {
                        NumberAnimation {
                            duration: 200
                            easing.type: Easing.Linear
                        }
                    }
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight | Qt.AlignBottom
                Button {
                    id: consoleBtn
                    text:"Show Console"
                    Layout.alignment: Qt.AlignLeft | Qt.AlignBottom
                    onPressed: {
                        if(consoleOutputWindow.visible) {
                            consoleOutputWindow.hide();
                        } else {
                            consoleOutputWindow.showNormal();
                        }
                    }
                }

                Button {
                    id: acceptBtn
                    text:"Clone"
                    Layout.alignment: Qt.AlignRight | Qt.AlignBottom
                    enabled: cloner.status == Cloner.PRECLONE && appName.text != ""
                    onPressed: {
                        successImage.index = Math.floor(Math.random()*successImage.images.length);
                        cloner.clone();
                    }
                }
                Button {
                    id: resetBtn
                    text:"Reset"
                    Layout.alignment: Qt.AlignRight | Qt.AlignBottom
                    enabled: cloner.status == Cloner.POSTCLONE
                    onPressed: {
                        platformName.text=""
                        cloner.reset();
                    }
                }
                Button {
                    id: quitBtn
                    text:"Quit"
                    Layout.alignment: Qt.AlignRight | Qt.AlignBottom
                    enabled: cloner.status == Cloner.PRECLONE || cloner.status == Cloner.POSTCLONE
                    onPressed: {
                        Qt.callLater(Qt.quit)
                    }
                }
            }


        }

    }

    FolderDialog {
        id: projectRootChooser

    }

    Settings {
        id: settings
        property alias projectRootDir: projectRootChooser.currentFolder
    }

    Cloner {
        id:cloner
        cloneToDirectory: usePlatformDirCheckBox.checked
                          ? projectRoot.text+"/"+projectName.text+"/"+appName.text
                          : projectRoot.text+"/"+projectName.text
        projectName: projectName.text
        platformName: usePlatformDirCheckBox.checked ? platformName.text : projectName.text
        applicationName: appName.text
        copyTopLevel: copyTopLevelCheckBox.checked
        usePlatformDir: usePlatformDirCheckBox.checked
    }
}

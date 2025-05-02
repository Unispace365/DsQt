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
    Item {
        id: toplevel
        width: root.width
        height: root.height
        property int labelWidth: 150

        ColumnLayout {
            anchors.fill: toplevel
            anchors.margins: 10
            spacing: 5
            Text {
                Layout.alignment: Qt.AlignTop
                id: title
                text: "DsQt (DsCute) ProjectCloner"
            }

            RowLayout {
                Layout.alignment: Qt.AlignTop
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
            RowLayout {
                Text {
                    Layout.preferredWidth: toplevel.labelWidth
                    text: "Main Name:"
                }
                Text {
                    id: appName
                    Layout.alignment: Qt.AlignTop
                    Layout.minimumWidth: 300
                    text: projectName.text.replace(/\ /,"")
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
                    property string inName: projectName.text.replace(/\ /,"")
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
                    text: projectRoot.text+"/" + appName.text
                }
            }
            ColumnLayout {
                Layout.preferredWidth: childrenRect.width
                Layout.topMargin: 20
                Layout.alignment: Qt.AlignHCenter
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
                        projectName.text=""
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
        location: "sets.stngs"
        property alias projectRootDir: projectRootChooser.currentFolder
    }

    Cloner {
        id:cloner
        cloneToDirectory: projectRoot.text+"/"+appName.text
        projectName: projectName.text
        applicationName: appName.text
    }
}

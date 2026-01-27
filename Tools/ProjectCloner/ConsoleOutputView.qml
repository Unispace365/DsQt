// ConsoleView.qml
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    //property var consoleOutput // Set this to your ConsoleOutput instance
    property alias autoScroll: autoScrollCheckBox.checked
    property color backgroundColor: "#1e1e1e"
    property color textColor: "#cccccc"
    property color timestampColor: "#6A9955"
    property color debugColor: "#569CD6"
    property color warningColor: "#CE9178"
    property color errorColor: "#F44747"

    color: backgroundColor

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 5
        spacing: 5

        // Toolbar
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            Rectangle {
                color: root.textColor
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            Button {
                text: "Clear"
                onClicked: ConsoleOutput.clear()
            }

            CheckBox {
                id: autoScrollCheckBox
                text: "Auto-scroll"

                checked: true
            }

            TextField {
                id: searchField
                Layout.fillWidth: true
                placeholderText: "Search..."

            }

            Label {
                text: listView.count + " messages"
                color: root.backgroundColor
            }
        }

        // Console output
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: listView
                model: ConsoleOutput ? ConsoleOutput.model : null
                clip: true

                delegate: ItemDelegate {
                    id: delegateItem
                    required property string message
                    required property int index
                    width: listView.width
                    height: textItem.implicitHeight + 4

                    background: Rectangle {
                        color: delegateItem.index % 2 ? Qt.darker(root.backgroundColor, 1.1) : root.backgroundColor
                    }

                    visible: searchField.text === "" ||
                            delegateItem.message.toLowerCase().includes(searchField.text.toLowerCase())

                    Text {
                        id: textItem
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: 5

                        text: formatMessage(delegateItem.message)
                        textFormat: Text.RichText
                        color: root.textColor
                        font.family: "Consolas, Monaco, monospace"
                        font.pointSize: 13
                        wrapMode: Text.Wrap

                        function formatMessage(msg) {
                            // Color-code based on message type
                            let formatted = msg;

                            // Highlight timestamp
                            formatted = formatted.replace(/^(\d{2}:\d{2}:\d{2}\.\d{3})/,
                                '<span style="color:' + root.timestampColor + '">$1</span>');

                            // Highlight message types
                            formatted = formatted.replace(/\[DEBUG\]/,
                                '<span style="color:' + root.debugColor + '">[DEBUG]</span>');
                            formatted = formatted.replace(/\[WARNING\]/,
                                '<span style="color:' + root.warningColor + '">[WARNING]</span>');
                            formatted = formatted.replace(/\[(CRITICAL|ERROR|FATAL)\]/,
                                '<span style="color:' + root.errorColor + '">[$1]</span>');

                            return formatted;
                        }
                    }

                    /*MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.RightButton
                        onClicked: contextMenu.popup()
                    }*/

                    /*Menu {
                        id: contextMenu
                        MenuItem {
                            text: "Copy"
                            onTriggered: {
                                let plainText = delegateItem.message.replace(/<[^>]*>/g, '');
                                delegateItem.textItem.
                            }
                        }
                    }*/
                }

                // Auto-scroll to bottom
                Connections {
                    target: ConsoleOutput
                    function onNewMessage(message) {
                        if (autoScrollCheckBox.checked) {
                            listView.positionViewAtEnd();
                        }
                    }
                }
            }
        }
    }
}

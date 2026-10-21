pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Dsqt.Waffles

// Shared loading, empty and failure treatment. Diagnostics stay in the application log.
// Visual reference: White Label Waffles, Figma 47:1217 / 83:4209.
Rectangle {
    id: root
    objectName: "contentState"
    property var viewer: null
    property string contentState: viewer ? viewer.loadState : "empty"
    property string title: contentState === "loading" ? qsTr("Loading content") : qsTr("Missing content")
    property string message: contentState === "loading" ? qsTr("Please wait…")
        : contentState === "empty" ? qsTr("There is no content to display here.")
        : qsTr("Sorry! We couldn’t load this piece of media. Please try again.")
    property string actionText: viewer && contentState === "error" ? qsTr("Try again") : ""
    signal actionRequested()
    visible: contentState !== "ready"
    color: DsTheme.surface
    clip: true
    Accessible.role: Accessible.StaticText
    Accessible.name: title + ". " + message

    Flickable {
        anchors.fill: parent
        contentHeight: Math.max(height, content.height + DsTheme.dp(32))
        boundsBehavior: Flickable.StopAtBounds
        ColumnLayout {
            id: content
            width: Math.max(0, Math.min(parent.width - DsTheme.dp(48), DsTheme.dp(400)))
            x: (parent.width - width) / 2
            y: Math.max(DsTheme.dp(16), (parent.height - height) / 2)
            spacing: DsTheme.dp(20)
            RowLayout {
                Layout.fillWidth: true
                spacing: DsTheme.dp(12)
                Image {
                    Layout.preferredWidth: DsTheme.dp(24)
                    Layout.preferredHeight: DsTheme.dp(24)
                    visible: root.contentState !== "loading"
                    source: "qrc:/qt/qml/Dsqt/Waffles/data/images/waffles/missing_content.svg"
                    fillMode: Image.PreserveAspectFit
                }
                BusyIndicator {
                    Layout.preferredWidth: DsTheme.dp(32)
                    Layout.preferredHeight: DsTheme.dp(32)
                    visible: root.contentState === "loading"
                    running: visible && root.visible
                }
                Text {
                    Layout.fillWidth: true
                    text: root.title
                    textFormat: Text.PlainText
                    color: DsTheme.surfaceText
                    font.family: "Roboto"
                    font.weight: Font.Light
                    font.pixelSize: DsTheme.dp(24)
                    font.letterSpacing: DsTheme.dp(1)
                    wrapMode: Text.Wrap
                }
            }
            Text {
                Layout.fillWidth: true
                text: root.message
                textFormat: Text.PlainText
                color: DsTheme.surfaceText
                font.family: "Roboto"
                font.weight: Font.Light
                font.pixelSize: DsTheme.dp(16)
                wrapMode: Text.Wrap
            }
            Button {
                id: action
                objectName: "contentStateAction"
                visible: root.actionText.length > 0
                text: root.actionText
                Layout.minimumHeight: DsTheme.dp(44)
                Layout.minimumWidth: DsTheme.dp(112)
                Accessible.name: text
                contentItem: Text {
                    text: action.text
                    textFormat: Text.PlainText
                    color: DsTheme.surfaceText
                    font.family: "Roboto"
                    font.pixelSize: DsTheme.dp(16)
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: action.down ? DsTheme.track : DsTheme.surfaceVariant
                    radius: DsTheme.dp(6)
                    border.width: action.activeFocus ? 2 : 1
                    border.color: action.activeFocus ? DsTheme.accent : DsTheme.track
                }
                onClicked: {
                    if (root.viewer) root.viewer.retry();
                    root.actionRequested();
                }
            }
        }
    }
}

pragma ComponentBehavior: Bound
pragma ListPropertyAssignBehavior: ReplaceIfNotDefault
import QtQuick
import QtQuick.Dialogs
import Dsqt.Waffles

DsWhiteboard {
    id: board
    controls: [WhiteboardControls {}]
    onSaveRequested: saveDialog.open()
    onSaveFailed: (file, message) => {
        errorDialog.text = message;
        errorDialog.open();
    }
    onShownChanged: {
        if (!shown) {
            saveDialog.close();
            errorDialog.close();
        }
    }
    FileDialog {
        id: saveDialog
        title: qsTr("Save whiteboard")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("PNG images (*.png)")]
        defaultSuffix: "png"
        onAccepted: board.save(selectedFile)
    }
    MessageDialog {
        id: errorDialog
        title: qsTr("Could not save whiteboard")
        buttons: MessageDialog.Ok
    }
}

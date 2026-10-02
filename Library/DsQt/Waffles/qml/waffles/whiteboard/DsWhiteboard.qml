pragma ComponentBehavior: Bound
pragma ListPropertyAssignBehavior: ReplaceIfNotDefault
import QtQuick
import Dsqt.Waffles

// Drawing/controller contract. Visual controls receive this object as signalObject.
Item {
    id: board
    property bool shown: true
    property list<DsControlSet> controls
    property var model: null
    property var config: ({})
    property bool solidBackground: true
    property color backgroundColor: "white"
    property alias tool: canvas.tool
    property alias strokeWidth: canvas.size
    property alias strokeColor: canvas.color
    property alias pathCaching: canvas.pathCaching
    property alias brushCenterAlpha: canvas.brushCenterAlpha
    property alias brushFalloff: canvas.brushFalloff
    property alias brushMinLayers: canvas.brushMinLayers
    property alias brushMaxLayers: canvas.brushMaxLayers
    readonly property bool canUndo: canvas.canUndo
    readonly property bool canRedo: canvas.canRedo
    readonly property bool hasDrawing: canvas.hasDrawing
    readonly property bool drawing: canvas.drawing
    readonly property bool saving: canvas.saving
    readonly property DsWhiteboardCanvas drawingCanvas: canvas
    readonly property Item captureItem: drawingSurface

    signal closeRequested()
    signal saveRequested()
    signal saved(url file)
    signal saveFailed(url file, string message)

    visible: shown
    enabled: shown
    onShownChanged: if (!shown) canvas.finishStrokes()

    function undo() { canvas.undo(); }
    function redo() { canvas.redo(); }
    function clear() { canvas.clear(); }
    function close() {
        canvas.finishStrokes();
        board.shown = false;
        board.closeRequested();
    }
    function requestSave() {
        canvas.finishStrokes();
        board.saveRequested();
    }
    function save(file) { return canvas.save(file, board.solidBackground, board.backgroundColor); }
    function addShape(tool, x, y, width, height, color, filled, strokeWidth, angle = 0) {
        canvas.addShape(tool, x, y, width, height, color, filled, strokeWidth, angle);
    }

    // Controls own their visuals; the host provides edge placement and the command target.
    property list<DsControlSet> _attachedControls
    function syncControls() {
        for (const control of board._attachedControls) {
            if (control && board.controls.indexOf(control) < 0) {
                control.visible = false;
                control.parent = board;
                control.signalObject = null;
            }
        }
        board._attachedControls = Array.from(board.controls);
        for (const control of board.controls) {
            control.parent = controlLayer;
            control.signalObject = board;
            control.model = Qt.binding(() => board.model);
            control.config = Qt.binding(() => board.config);
            control.width = Qt.binding(() => Math.min(control.implicitWidth || board.width, board.width));
            control.height = Qt.binding(() => Math.min(control.implicitHeight || board.height, board.height));
            control.x = Qt.binding(() => board.controlX(control));
            control.y = Qt.binding(() => board.controlY(control));
            control.visible = Qt.binding(() => board.shown);
            control.opacity = 1;
        }
    }
    function controlX(control) {
        switch (control.edge) {
        case DsControlSet.LeftInner:
        case DsControlSet.LeftOuter: return control.offset + control.horizontalOffset;
        case DsControlSet.RightInner:
        case DsControlSet.RightOuter: return board.width - control.width - control.offset + control.horizontalOffset;
        default: return (board.width - control.width) / 2 + control.horizontalOffset;
        }
    }
    function controlY(control) {
        switch (control.edge) {
        case DsControlSet.TopInner:
        case DsControlSet.TopOuter: return control.offset + control.verticalOffset;
        case DsControlSet.BottomInner:
        case DsControlSet.BottomOuter: return board.height - control.height - control.offset + control.verticalOffset;
        default: return (board.height - control.height) / 2 + control.verticalOffset;
        }
    }
    onControlsChanged: Qt.callLater(board.syncControls)
    Component.onCompleted: board.syncControls()

    Item {
        id: drawingSurface
        anchors.fill: parent
        Rectangle {
            anchors.fill: parent
            color: board.backgroundColor
            visible: board.solidBackground
        }
        // Block pointer events from reaching underlying content, including transparent areas.
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            onWheel: wheel => wheel.accepted = true
        }
        DsWhiteboardCanvas {
            id: canvas
            anchors.fill: parent
            size: 6
            onSaved: file => board.saved(file)
            onSaveFailed: (file, message) => board.saveFailed(file, message)
        }
    }
    Item {
        id: controlLayer
        anchors.fill: parent
    }
}

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtTest
import Dsqt.Waffles

Item {
    id: root
    width: 1280
    height: 720
    property bool captured: false

    Component {
        id: bareBoard
        DsWhiteboard { width: 1280; height: 720 }
    }
    Component {
        id: defaultBoard
        Whiteboard { width: 1280; height: 720 }
    }
    Component {
        id: declaredCustomBoard
        Whiteboard {
            width: 1280; height: 720
            controls: [DsControlSet {
                edge: DsControlSet.TopInner
                implicitWidth: 100; implicitHeight: 50
            }]
        }
    }
    Component {
        id: customControl
        DsControlSet {
            id: custom
            edge: DsControlSet.TopInner
            implicitWidth: 120; implicitHeight: 60
            Button {
                objectName: "customUndo"
                anchors.fill: parent
                text: qsTr("Undo drawing")
                enabled: custom.signalObject?.canUndo ?? false
                onClicked: custom.signalObject.undo()
            }
        }
    }
    Component {
        id: stageComponent
        DsWaffleStage {
            width: 1280; height: 720
            glassEnabled: false
            launcherButtonVisible: false
            launcher: Component {
                Item {
                    property var stage
                    property var model
                    property bool shown: false
                }
            }
        }
    }

    TestCase {
        id: tests
        name: "Whiteboard"
        when: windowShown
        SignalSpy { id: savedSpy; signalName: "saved" }
        SignalSpy { id: failureSpy; signalName: "saveFailed" }
        SignalSpy { id: closeSpy; signalName: "closeRequested" }
        SignalSpy { id: requestSpy; signalName: "saveRequested" }

        function makeBoard(component = bareBoard) {
            const board = createTemporaryObject(component, root);
            verify(!!board, "Component exists");
            if (board.controls.length) tryCompare(board.controls[0], "opacity", 1);
            verify(waitForRendering(board));
            return board;
        }
        function draw(board, y = 100) {
            mousePress(board.drawingCanvas, 100, y);
            mouseMove(board.drawingCanvas, 200, y);
            mouseRelease(board.drawingCanvas, 300, y);
            tryCompare(board, "canUndo", true);
        }
        function exportBoard(board, filename) {
            savedSpy.target = board;
            savedSpy.clear();
            const file = testFiles.outputFile(filename);
            verify(board.save(file));
            tryCompare(savedSpy, "count", 1);
            compare(testFiles.imageSize(file).width, board.width);
            compare(testFiles.imageSize(file).height, board.height);
            return file;
        }
        function test_historyBranch() {
            const board = makeBoard();
            draw(board);
            board.undo();
            tryCompare(board, "hasDrawing", false);
            tryCompare(board, "canRedo", true);
            board.redo();
            tryCompare(board, "hasDrawing", true);
            board.undo();
            draw(board, 140);
            tryCompare(board, "canRedo", false);
            board.clear();
            tryCompare(board, "canUndo", false);
            tryCompare(board, "canRedo", false);
            tryCompare(board, "hasDrawing", false);
        }
        function test_penPixels() {
            const board = makeBoard();
            board.strokeColor = "#ff0000";
            board.strokeWidth = 20;
            draw(board);
            const file = exportBoard(board, "pen.png");
            compare(testFiles.pixel(file, 200, 100), "#ff0000");
            compare(testFiles.pixel(file, 200, 200), "#ffffff");
            // The bottom area has no toolbar in exported images.
            compare(testFiles.pixel(file, 640, 600), "#ffffff");
        }
        function test_highlighterPixels() {
            const board = makeBoard();
            board.tool = DsWhiteboardTool.Highlighter;
            board.strokeColor = "#ff0000";
            board.strokeWidth = 30;
            board.solidBackground = false;
            draw(board);
            const file = exportBoard(board, "highlighter.png");
            const color = testFiles.pixel(file, 200, 100);
            verify(color.r > 0.95);
            verify(color.a > 0.3 && color.a < 0.4);
            compare(testFiles.pixel(file, 200, 200).a, 0);
        }
        function test_eraserUndoPixels() {
            const board = makeBoard();
            board.strokeColor = "#ff0000";
            board.strokeWidth = 20;
            draw(board);
            board.tool = DsWhiteboardTool.Eraser;
            board.strokeWidth = 50;
            mousePress(board.drawingCanvas, 200, 70);
            mouseMove(board.drawingCanvas, 200, 100);
            mouseRelease(board.drawingCanvas, 200, 130);
            tryCompare(board, "canUndo", true);
            let file = exportBoard(board, "erased.png");
            compare(testFiles.pixel(file, 200, 100), "#ffffff");
            board.undo();
            file = exportBoard(board, "erase-undone.png");
            compare(testFiles.pixel(file, 200, 100), "#ff0000");
            board.redo();
            file = exportBoard(board, "erase-redone.png");
            compare(testFiles.pixel(file, 200, 100), "#ffffff");
        }
        function test_toolsFrozenDuringStroke() {
            const board = makeBoard();
            board.strokeColor = "#ff0000";
            board.strokeWidth = 20;
            mousePress(board.drawingCanvas, 100, 100);
            tryCompare(board, "drawing", true);
            tryCompare(board, "canUndo", false);
            board.tool = DsWhiteboardTool.None;
            board.strokeColor = "#0000ff";
            mouseMove(board.drawingCanvas, 200, 100);
            mouseRelease(board.drawingCanvas, 300, 100);
            tryCompare(board, "drawing", false);
            const file = exportBoard(board, "frozen-tool.png");
            compare(testFiles.pixel(file, 200, 100), "#ff0000");
        }
        function test_multitouch() {
            const board = makeBoard();
            const sequence = touchEvent(board.drawingCanvas);
            sequence.press(0, board.drawingCanvas, 100, 100)
                    .press(1, board.drawingCanvas, 100, 200).commit();
            tryCompare(board, "drawing", true);
            sequence.move(0, board.drawingCanvas, 200, 100)
                    .move(1, board.drawingCanvas, 200, 200).commit();
            sequence.release(0, board.drawingCanvas, 300, 100)
                    .release(1, board.drawingCanvas, 300, 200).commit();
            tryCompare(board, "drawing", false);
            board.undo();
            compare(board.canUndo, true);
            board.undo();
            compare(board.hasDrawing, false);
        }
        function test_shapesResizeAndUndoLimit() {
            const board = makeBoard();
            board.addShape(DsWhiteboardTool.Square, 100, 100, 80, 80, "#ff0000", true, 6);
            for (let i = 0; i < 70; ++i)
                board.addShape(DsWhiteboardTool.Circle, 400 + i * 2, 200, 10, 10, "#0000ff", true, 2);
            exportBoard(board, "before-resize.png");
            board.width = 1200;
            board.height = 680;
            let file = exportBoard(board, "after-resize.png");
            compare(testFiles.pixel(file, 140, 140), "#ff0000");
            let count = 0;
            while (board.canUndo && count < 100) { board.undo(); ++count; }
            compare(count, 64);
            compare(board.hasDrawing, true);
            file = exportBoard(board, "undo-limit.png");
            compare(testFiles.pixel(file, 140, 140), "#ff0000");
            board.clear();
            file = exportBoard(board, "cleared.png");
            compare(testFiles.pixel(file, 140, 140), "#ffffff");
        }
        function test_declarativeControlReplacement() {
            const board = makeBoard(declaredCustomBoard);
            compare(board.controls.length, 1);
            compare(board.controls[0].edge, DsControlSet.TopInner);
            compare(board.controls[0].signalObject, board);
        }
        function test_replaceControlsKeepsDrawing() {
            const board = makeBoard(defaultBoard);
            draw(board);
            const replacement = createTemporaryObject(customControl, root);
            verify(!!replacement, "Object exists");
            board.controls = [replacement];
            tryCompare(replacement, "signalObject", board);
            tryCompare(board, "hasDrawing", true);
            const undo = findChild(replacement, "customUndo");
            verify(!!undo, "Object exists");
            mouseClick(undo);
            tryCompare(board, "hasDrawing", false);
            tryCompare(board, "canRedo", true);
        }
        function test_defaultControls() {
            const board = makeBoard(defaultBoard);
            const highlighter = findChild(board, "highlighterButton");
            verify(!!highlighter, "Object exists");
            mouseClick(highlighter);
            tryCompare(board, "tool", DsWhiteboardTool.Highlighter);
            const background = findChild(board, "backgroundButton");
            verify(!!background, "Object exists");
            mouseClick(background);
            tryCompare(board, "solidBackground", false);
            draw(board);
            const undo = findChild(board, "undoButton");
            verify(!!undo, "Object exists");
            mouseClick(undo);
            tryCompare(board, "hasDrawing", false);
            const redo = findChild(board, "redoButton");
            verify(!!redo, "Object exists");
            mouseClick(redo);
            tryCompare(board, "hasDrawing", true);
        }
        function test_closePreservesDrawing() {
            const board = makeBoard(defaultBoard);
            draw(board);
            closeSpy.target = board;
            closeSpy.clear();
            const close = findChild(board, "closeButton");
            verify(!!close, "Object exists");
            mouseClick(close);
            tryCompare(closeSpy, "count", 1);
            tryCompare(board, "shown", false);
            board.shown = true;
            tryCompare(board, "hasDrawing", true);
            const file = exportBoard(board, "reopened.png");
            compare(testFiles.pixel(file, 200, 100), "#000000");
        }
        function test_saveRequested() {
            const board = makeBoard();
            requestSpy.target = board;
            requestSpy.clear();
            board.requestSave();
            compare(requestSpy.count, 1);
        }
        function test_saveFailure() {
            const board = makeBoard();
            failureSpy.target = board;
            failureSpy.clear();
            compare(board.save("https://example.com/drawing.png"), false);
            compare(failureSpy.count, 1);
            compare(board.saving, false);
        }
        function test_stageLifecycle() {
            const stage = createTemporaryObject(stageComponent, root);
            verify(!!stage, "Component exists");
            const board = stage.openWhiteboard();
            verify(!!board, "Object exists");
            tryCompare(stage, "whiteboardShown", true);
            draw(board);
            stage.closeWhiteboard();
            tryCompare(stage, "whiteboardShown", false);
            compare(stage.openWhiteboard(), board);
            tryCompare(board, "hasDrawing", true);
            stage.clearForeground();
            tryCompare(stage, "whiteboardShown", false);
            tryCompare(board, "hasDrawing", false);
        }
        function test_visualPreview() {
            const board = makeBoard(defaultBoard);
            board.width = 3840;
            board.height = 2160;
            board.scale = 1 / 3;
            board.strokeWidth = 48;
            board.strokeColor = "#ffd30f";
            mousePress(board.drawingCanvas, 1140, 900);
            mouseMove(board.drawingCanvas, 1440, 840);
            mouseMove(board.drawingCanvas, 1590, 1020);
            mouseRelease(board.drawingCanvas, 1800, 990);
            tryCompare(board, "hasDrawing", true);
            root.captured = false;
            verify(board.grabToImage(result => {
                root.captured = result.saveToFile(testFiles.outputFile("whiteboard-preview.png"));
            }, Qt.size(1280, 720)));
            tryCompare(root, "captured", true);
        }
    }
}

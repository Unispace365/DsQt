import QtQuick

Item {
    enum ViewerType {
        TitledMediaViewer,
        Launcher,
        PresentationController,
        FullscreenController,
        Unknown
    }
    property var signalObject: null
    property int viewerType: DsViewer.ViewerType.Unknown
    property int viewerWidth: 400
    property int viewerHeight: 300

    // The composite "slot" the stage assigns: a stage-sized item holding everything
    // below this viewer in z-order. Subclasses sample their own rect from it for the glass.
    property Item backdropSource: null
}

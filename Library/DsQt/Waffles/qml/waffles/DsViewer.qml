import QtQuick

Item {
    id: dsViewer
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

    // The item the stage captures into other viewers' slots. Must sit at (0,0) within the
    // viewer and must NOT contain any glass (so captures don't recurse). Subclasses that have
    // a glass layer override this to point at their glass-free content layer.
    property Item captureItem: dsViewer
}

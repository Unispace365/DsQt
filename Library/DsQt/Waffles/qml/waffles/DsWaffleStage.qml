import QtQuick
import Dsqt.Waffles



Item {
    id: wafflesRoot
    property Component launcher: Component { DsTestLauncher {} }
    property Component viewer: Component { TitledMediaViewer {} }
    property Component fullscreenController: Qt.createComponent("FullscreenController.qml")
    property Component presentationController: Qt.createComponent("PresentationController.qml")
    property bool menuShown: false

    // --- Glass / blurred-backdrop config (global defaults; per-viewer overridable) ---
    property bool  glassEnabled: true
    property color glassTint: "#191F25"
    property real  glassTintOpacity: 0.8
    property real  glassBlur: 0.5
    property int   glassBlurMax: 32
    property real  glassRadius: 12

    // Content captured as the bottom of the glass stack (e.g. an animated background).
    property alias backgroundContent: backdrop.data

    // Live composite "slots" (stage-sized textures), one per viewer that has something above it.
    property var _glassSlots: []

    onGlassEnabledChanged: rebuildGlass()

    Component.onCompleted: {
        createMenu()
    }

    TapHandler {

    }

    // C[0] of the glass chain: everything below the viewer layer (e.g. animated background).
    Item {
        id: backdrop
        anchors.fill: wafflesRoot
    }

    // Off-screen holder for the cumulative composite chain. Each slot is a stage-sized
    // texture of everything beneath one viewer. Kept invisible; its textures are pulled by
    // the visible per-viewer glass surfaces, which is what keeps the live chain updating.
    Item {
        id: glassSlotHolder
        anchors.fill: wafflesRoot
        visible: false
    }

    Item {
        id: topLayer
        anchors.fill: wafflesRoot
        // Keep viewers/launcher above any consumer content placed in the stage.
        z: 1
        onChildrenChanged: wafflesRoot.rebuildGlass()
    }

    // One link of the glass chain: composites the previous slot with the viewer just below.
    Component {
        id: glassSlotComponent
        Item {
            id: slot
            property Item prevSource: null
            property Item viewerBelow: null
            anchors.fill: parent
            ShaderEffectSource {
                anchors.fill: parent
                sourceItem: slot.prevSource
                live: true
                hideSource: false
            }
            // Capture the viewer's full extent (childrenRect), not just its root bounds,
            // so control sets that overflow the media area (e.g. the title bar above) are
            // composited into the slots that viewers above will sample.
            ShaderEffectSource {
                x: slot.viewerBelow ? slot.viewerBelow.x + slot.viewerBelow.childrenRect.x : 0
                y: slot.viewerBelow ? slot.viewerBelow.y + slot.viewerBelow.childrenRect.y : 0
                width: slot.viewerBelow ? slot.viewerBelow.childrenRect.width : 0
                height: slot.viewerBelow ? slot.viewerBelow.childrenRect.height : 0
                sourceItem: slot.viewerBelow
                sourceRect: slot.viewerBelow
                            ? Qt.rect(slot.viewerBelow.childrenRect.x, slot.viewerBelow.childrenRect.y,
                                      slot.viewerBelow.childrenRect.width, slot.viewerBelow.childrenRect.height)
                            : Qt.rect(0, 0, 0, 0)
                live: true
                hideSource: false
            }
        }
    }

    //functions && _privates
    QtObject {
        id: _private
        property var launcher: null
    }

    function createMenu() {
        if (launcher.status == Component.Ready)
            completeMenu()
        else
            launcher.statusChanged.connect(completeMenu)
    }

    function completeMenu() {
        if(launcher.status == Component.Ready) {
            _private.launcher = launcher.createObject(topLayer,{"opacity":1, "stage":wafflesRoot});
            if(_private.launcher == null)
            {
                console.log("Error creating menu");
            }
        } else if (launcher.status === Component.Error) {
            console.log("Error loading component:", launcher.errorString());
        }
    }

    function createViewer(viewerProps: var) {
        if (viewer.status == Component.Ready)
            completeViewer(viewerProps)
        else
            viewer.statusChanged.connect(()=>{completeViewer(viewerProps);})
    }

    function completeViewer(viewerProps: var) {
        if(viewer.status == Component.Ready) {
            let viewerInstance = viewer.createObject(topLayer,viewerProps);
            if(viewerInstance == null)
            {
                console.log("Error creating viewer");
            }
        } else if (viewer.status === Component.Error) {
            console.log("Error loading component:", viewer.errorString());
        }
    }

    function openViewer(viewerProps: var){
        createViewer(viewerProps);
    }

    // Rebuilds the cumulative glass chain. Each topLayer child is assigned a backdropSource
    // (a slot holding everything painted below it); the next link composites that slot with
    // the child itself. Children paint in array order, which is our z-order. Movement and
    // animated content update for free via the live ShaderEffectSources; only add/remove/
    // reorder needs a rebuild.
    function rebuildGlass() {
        for (let i = 0; i < _glassSlots.length; i++) {
            if (_glassSlots[i])
                _glassSlots[i].destroy();
        }
        _glassSlots = [];

        let parts = topLayer.children;

        // Glass disabled: drop every viewer's source so the live chain stops entirely.
        if (!glassEnabled) {
            for (let j = 0; j < parts.length; j++) {
                let q = parts[j];
                if (q && ("backdropSource" in q))
                    q.backdropSource = null;
            }
            return;
        }

        let prevSource = backdrop;
        for (let i = 0; i < parts.length; i++) {
            let p = parts[i];
            if (p && ("backdropSource" in p))
                p.backdropSource = prevSource;

            if (i < parts.length - 1) {
                let slot = glassSlotComponent.createObject(glassSlotHolder, {
                    "prevSource": prevSource,
                    "viewerBelow": p
                });
                _glassSlots.push(slot);
                prevSource = slot;
            }
        }
    }

    signal hideControlsExcept(child:var)

    TapHandler {
        target: null
        onTapped: (point,button)=>{
            if(tapCount == 2){
                _private.launcher.x = point.position.x
                _private.launcher.y = point.position.y
            }
            let inView = null;
            for(let i=0;i<topLayer.children.length;i++){
              let child = topLayer.children[i];
              if(child.contains(child.mapFromGlobal(point.globalPosition))){
                  inView = child;
              }

            }
            wafflesRoot.hideControlsExcept(inView);

        }
    }
}

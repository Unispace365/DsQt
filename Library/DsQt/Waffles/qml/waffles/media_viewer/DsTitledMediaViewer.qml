import QtQuick 2.15
import QtMultimedia
import Dsqt.Core
import Dsqt.Waffles

DsViewer {
    id: root
    viewerType: DsViewer.ViewerType.TitledMediaViewer
    property list<DsControlSet> controls

    property var model: null
    readonly property var config: configObj

    property DsWaffleStage stage: null
    //@brief this defines this objects default config.
    //the keys of theconfig map to the property of the TitledMediaViewers config object which is passed to controlSets.
    //that config object is filled with the values from your model. If you are using custom contolSets that
    //know about your model, then you can ignore this and just get data directly from the model.
    //if you setup your model's keys to match these values you won't need to set modelConfig.
    //if your model's data for the given property is different than the default you can override
    //this by copying the object and changing the values to match your model's keys.
    property var modelConfig: {
        "media": "media",
        "title": "title",
        "subtitle" : "subtitle"
    }
    property alias mediaViewer: mediaView.viewer

    // Fill mode for the media. PreserveAspectFit letterboxes, revealing the glass behind.
    property int mediaFillMode: Image.PreserveAspectCrop

    // --- Glass config: defaults to the stage globals, override per-instance if needed ---
    property bool  glassEnabled:     stage ? stage.glassEnabled : true
    property color glassTint:        stage ? stage.glassTint : "#191F25"
    property real  glassTintOpacity: stage ? stage.glassTintOpacity : 0.8
    property real  glassBlur:        stage ? stage.glassBlur : 0.5
    property int   glassBlurMax:     stage ? stage.glassBlurMax : 32
    property real  glassRadius:      stage ? stage.glassRadius : 12
    // Background shown behind the media when glass is off (developer configurable).
    property color glassFallbackColor: glassTint

    // Selected viewers show their title/controls; unselected ones hide them and the glass
    // shrinks to just the media area. The stage sets this and raises the selected viewer.
    property bool selected: false
    onSelectedChanged: selected ? showControls() : hideControls()

    // The stage captures this (glass + content) into the slots viewers above sample, so an
    // upper viewer's glass shows the lower viewers WITH their glass — the compound look. Slots
    // are flat (leaf sources only), so this nesting has no depth chain that could blank out.
    captureItem: contentLayer


    QtObject {
        id: configObj
        property var media: model[root.modelConfig.media]
        property var title: model[root.modelConfig.title]
        property var subtitle: model[root.modelConfig.subtitle]
    }

    // Glass settings handed to the control sets so their backgrounds can match (or diverge
    // from) the media area. viewerX/Y are the viewer's stage position; nested controls add
    // their own offset within the viewer to sample the right region. refresh recomputes that
    // offset when the viewer moves/resizes.
    QtObject {
        id: glassContext
        // Controls sample the backdrop leaf (not this viewer's slot) so capturing the controls
        // into other viewers' slots can't recurse through the control glass.
        property Item  source: root.stage ? root.stage.glassBackdrop : null
        property Item  viewerItem: root
        property real  viewerX: root.x
        property real  viewerY: root.y
        property bool  enabled: root.glassEnabled
        property color tint: root.glassTint
        property real  tintOpacity: root.glassTintOpacity
        property real  blur: root.glassBlur
        property int   blurMax: root.glassBlurMax
        property real  refresh: root.x + root.y + root.width + root.height
    }

    width: mediaView.width;
    height: mediaView.height;

    Component.onCompleted: { setControls(); if (!selected) hideControls(); }
    onControlsChanged: setControls()


    //this function positions the control in their respective edges
    function setControls() {
        for(var i=0; i < controls.length; i++){
            let ctrl = controls[i];
            ctrl.config = config
            ctrl.model = model
            ctrl.glass = glassContext
            let edge = ctrl.edge;
            switch(edge) {
            case DsControlSet.Edge.TopOuter:
                ctrl.parent = Qt.binding(()=>{return topOuter;})
                ctrl.width = Qt.binding(()=>{return topOuter.width;})
                topOuter.offset = Qt.binding(()=>{return ctrl.offset;})
                topOuter.enabled = true;
                break;
            case DsControlSet.Edge.TopInner:
                ctrl.parent = Qt.binding(()=>{return topInner;})
                ctrl.width = Qt.binding(()=>{return topInner.width;})
                topInner.offset = Qt.binding(()=>{return ctrl.offset;})
                topInner.enabled = true;
                break;
            case DsControlSet.Edge.BottomOuter:
                ctrl.parent = Qt.binding(()=>{return bottomOuter;})
                ctrl.width = Qt.binding(()=>{return bottomOuter.width;})
                bottomOuter.offset = Qt.binding(()=>{return ctrl.offset;})
                bottomOuter.enabled = true;
                break;
            case DsControlSet.Edge.BottomInner:
                ctrl.parent = Qt.binding(()=>{return bottomInner;})
                ctrl.width = Qt.binding(()=>{return bottomInner.width;})
                bottomInner.offset = Qt.binding(()=>{return ctrl.offset;})
                bottomInner.enabled = true;
                break;
            case DsControlSet.Edge.LeftOuter:
                ctrl.parent = Qt.binding(()=>{return leftOuter;})
                ctrl.height = Qt.binding(()=>{return leftOuter.height;})
                leftOuter.offset = Qt.binding(()=>{return ctrl.offset;})
                leftOuter.enabled = true;
                break;
            case DsControlSet.Edge.LeftInner:
                ctrl.parent = Qt.binding(()=>{return leftInner;})
                ctrl.height = Qt.binding(()=>{return leftInner.height;})
                leftInner.offset = Qt.binding(()=>{return ctrl.offset;})
                leftInner.enabled = true;
                break;
            case DsControlSet.Edge.RightOuter:
                ctrl.parent = Qt.binding(()=>{return rightOuter;})
                ctrl.height = Qt.binding(()=>{return rightOuter.height;})
                rightOuter.offset = Qt.binding(()=>{return ctrl.offset;})
                rightOuter.enabled = true;
                break;
            case DsControlSet.Edge.RightInner:
                ctrl.parent = Qt.binding(()=>{return rightInner;})
                ctrl.height = Qt.binding(()=>{return rightInner.height;})
                rightInner.offset = Qt.binding(()=>{return ctrl.offset;})
                rightInner.enabled = true;
                break;
            case DsControlSet.Edge.Center:
                ctrl.parent = Qt.binding(()=>{return mediaView;})
                ctrl.width = Qt.binding(()=>{return mediaView.width;})
                ctrl.height = Qt.binding(()=>{return mediaView.height;})
                break;

            case DsControlSet.Edge.CenterBack:
                ctrl.parent = Qt.binding(()=>{return background;})
                background.horizontalOffset = Qt.binding(()=>{return ctrl.horizontalOffset;})
                background.verticalOffset = Qt.binding(()=>{return ctrl.verticalOffset;})
                break;
            }
        }
    }
    // The viewer's captured appearance: its glass panel + media + controls. The stage captures
    // THIS into the slots that viewers above sample, so an upper viewer's glass blurs the lower
    // viewers together with their own glass (compound). childrenRect spans the glass + controls.
    Item {
        id: contentLayer
        anchors.fill: parent

        // Frosted-glass panel behind this viewer — the title bar (when present) plus the media
        // area. Explicit geometry keeps it independent of declaration/layout order.
        DsGlassBackground {
            id: viewerGlass
            z: -1
            x: 0
            width: mediaView.width
            y: (root.selected && topOuter.enabled) ? topOuter.y : 0
            height: (root.selected && topOuter.enabled) ? (mediaView.height - topOuter.y) : mediaView.height
            source: root.backdropSource
            sampleX: root.x + viewerGlass.x
            sampleY: root.y + viewerGlass.y
            blurEnabled: root.glassEnabled
            tint: root.glassTint
            tintOpacity: root.glassTintOpacity
            blur: root.glassBlur
            blurMax: root.glassBlurMax
            fallbackColor: root.glassFallbackColor
            // Card is rounded at the top only (matching the title bar); square at the bottom.
            topLeftRadius: root.glassRadius
            topRightRadius: root.glassRadius
            bottomLeftRadius: 0
            bottomRightRadius: 0
        }

        // Consumes presses over the viewer body so a viewer on top blocks touches/clicks from
        // reaching viewers or the gallery beneath it (a MouseArea accepts the press, unlike a
        // PointerHandler). Also drags the viewer by its body and raises it on press. Sits below
        // the media/controls so they (e.g. a web viewer or buttons) still get input first.
        MouseArea {
            anchors.fill: mediaView
            acceptedButtons: Qt.AllButtons
            drag.target: root
            onPressed: (mouse) => { if (root.stage) root.stage.selectViewer(root) }
        }

        Item {
            id: background
            property real horizontalOffset:0
            property real verticalOffset:0
            anchors.horizontalCenter: mediaView.horizontalCenter
            anchors.verticalCenter: mediaView.verticalCenter
            anchors.horizontalCenterOffset: horizontalOffset
            anchors.verticalCenterOffset: verticalOffset
        }

        //this is the media view. It has several children that are designed to only show one based on the media
        //type. We should make this explicit so the we can use VectorImage and Web element as well.
        Item {
            id: mediaView
            width: root.viewerWidth;
            height: root.viewerHeight
            property alias viewer: viewer
            DsMediaViewer {
                id: viewer
                anchors.fill: parent
                media: root.config.media
                fillMode: root.mediaFillMode
                autoPlay: true
                visible: true // TEMP: media hidden to inspect the glass background
            }
        }

        //holders for the controlSets.
        Item {
            id:topOuter
            property real offset:0
            width: mediaView.width
            height: childrenRect.height;
            anchors.bottom: mediaView.top
            anchors.bottomMargin: offset
        }

        Item {
            id:bottomOuter
            property real offset:0
            width: mediaView.width
            height: childrenRect.height;
            anchors.top: mediaView.bottom
            anchors.topMargin: offset
        }

        Item {
            id:leftOuter
            property real offset:0
            width: childrenRect.width
            height: mediaView.height;
            anchors.right: mediaView.left
            anchors.rightMargin: offset
        }

        Item {
            id:rightOuter
            property real offset:0
            width: childrenRect.width
            height: mediaView.height;
            anchors.left: mediaView.right
            anchors.leftMargin: offset
        }

        Item {
            id:topInner
            property real offset:0
            width: mediaView.width
            height: childrenRect.height;
            anchors.top: mediaView.top
            anchors.topMargin: offset
        }

        Item {
            id:bottomInner
            property real offset:0
            width: mediaView.width
            height: childrenRect.height;
            anchors.bottom: mediaView.bottom
            anchors.bottomMargin: offset
        }

        Item {
            id:leftInner
            property real offset:0
            width: childrenRect.width
            height: mediaView.height;
            anchors.right: mediaView.left
            anchors.rightMargin: offset
        }

        Item {
            id:rightInner
            property real offset:0
            width: childrenRect.width
            height: mediaView.height;
            anchors.left: mediaView.right
            anchors.leftMargin: offset
        }
    }

    function hideControls() {
        for(var i=0; i <root.controls.length; i++){
            let ctrl = root.controls[i];
            ctrl.opacity = 0;
        }
    }

    function showControls() {
        for(var i=0; i <root.controls.length; i++){
            let ctrl = root.controls[i];
            ctrl.opacity = 1;
        }
    }

}

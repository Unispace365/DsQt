import QtQuick 2.15
import QtMultimedia
import Dsqt

DsViewer {
    id: root
    viewerType: DsViewer.ViewerType.TitledMediaViewer
    property list<DsControlSet> controls

    property var model: null

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


    QtObject {
        id: config
        property var media: model[root.modelConfig.media]
        property var title: model[root.modelConfig.title]
        property var subtitle: model[root.modelConfig.subtitle]
    }

    width: mediaView.width;
    height: mediaView.height;

    Component.onCompleted: setControls()
    onControlsChanged: setControls()


    //this function positions the control in their respective edges
    function setControls() {
        for(var i=0; i < controls.length; i++){
            let ctrl = controls[i];
            ctrl.config = config
            ctrl.model = model
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
        width: childrenRect.width;
        height: childrenRect.height;
        property alias viewer: viewer
        DsMediaViewer {
            id: viewer
            anchors.fill: parent
            media: root.source
            autoPlay: true
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

    //this is a rudimentary
    DragHandler {}

}

import QtQuick 2.15
import QtQuick.Controls
import QtQuick.Layouts
import Dsqt
import QtQuick.VirtualKeyboard
import QtQuick.VirtualKeyboard.Settings
import WafflesUx as Waffles
import "../viewer"

Item {
    anchors.fill: parent
    Component.onCompleted: {
        palette.button = "black"
        palette.light = "grey"
        palette.text = "white"
        VirtualKeyboardSettings.styleName = "waffles"
    }

    DsSettingsProxy {
        id:appProxy
        target:"app_settings"
    }

    Rectangle {
        color: "lightgrey"
        anchors.fill: parent
    }

    DsWaffleStage {
        id:stage1
        anchors.fill: parent
        viewer: Waffles.TitledMediaViewer {
            id: viewer
            signalObject: viewer
            stage: stage1
            function close() {
                viewer.destroy();
            }
        }
        Component.onCompleted: {
            stage1.createViewer({model:{title:"Demo Viewer",media:createResourceObj("file:///C:/Users/aubrey.francois/Documents/downstream/settings/waffles_dev/media/sample-hut-400x300.png",400,300,"image")},viewerWidth:400, viewerHeight:300});
        }
    }

    //c++ resource example
    /*
    valueOut.insert("filepath", QVariant::fromValue(resource.getAbsoluteFilePath()));
    valueOut.insert("thumbnailId", QVariant::fromValue(resource.getThumbnailId()));
    valueOut.insert("thumbnailFilepath", QVariant::fromValue(resource.getThumbnailFilePath()));
    valueOut.insert("duration", QVariant::fromValue(resource.getDuration()));
    auto type = resource.getTypeName();
    valueOut.insert("type", QVariant::fromValue(resource.getTypeName()));
    valueOut.insert("width", QVariant::fromValue(resource.getWidth()));
    valueOut.insert("height", QVariant::fromValue(resource.getHeight()));
    QRectF cropValue = resource.getCrop();
    valueOut.insert("crop", QVariantList::fromVector(
                                {cropValue.x(), cropValue.y(), cropValue.width(), cropValue.height()}));
    */

    //make javascript obj resource
    function createResourceObj(url,w,h,t){
        var resObj = {}
        resObj.filepath = url;
        resObj.thumbnailId = "";
        resObj.thumbnailFilepath = "";
        resObj.duration = 0;
        resObj.type = t;
        resObj.width = w;
        resObj.height = h;
        resObj.crop = [0,0,0,0]; //x,y,w,h
        return resObj;
    }

    /*InputPanel {
        id: inputPanel
        visible: true
        //set the aspect ratio of the keyboard to be the same as in style.qml in our custom style
        width: 402*4
        height: 216*4
        DragHandler {
            //(@NOTE:KEYBOARD) Prevent the keys from dragging the keyboard.
            //this is telling this Drag Handler that it can only steal
            //the grab from other DragHandlers
            grabPermissions: PointerHandler.CanTakeOverFromHandlersOfSameType
        }
    }*/

    DsClusterView {
        id: jawa
        manager: wookie
        menuConfig: appProxy.getObj("menu")
        menuModel: appProxy.getList("menu.item")

        DsQuickMenu {
        }
    }

    DsClusterManager {
        id: wookie
        enabled:true;
        minClusterTouchCount: 3
        minClusterSeperation: 500
        holdOpenOnTouch: true
        anchors.fill: parent
    }

}

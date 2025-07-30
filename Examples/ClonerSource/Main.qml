import QtQuick
import QtQuick.VirtualKeyboard

import Dsqt

DsAppBase {
    id: base

    rootSrc: Ds.env.expand("file:%APP%/qml/Sizer.qml")//Qt.resolvedUrl("file:DsqtApp/qml/Gallery.qml");
    Component.onCompleted: {

        //console.log("WOOOT!")
       // console.log(Qt.resolvedUrl("Sizer.qml"))
       // console.log(contentRoot["cms_root"]);
    }
}



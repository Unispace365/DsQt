import QtQuick
import QtQuick.VirtualKeyboard

import Dsqt

DSAppBase {
    id: base

    rootSrc: DS.env.expand("file:%APP%/qml/Gallery.qml")//Qt.resolvedUrl("file:DsqtApp/qml/Gallery.qml");
    Component.onCompleted: {

        //console.log("WOOOT!")
       // console.log(Qt.resolvedUrl("Sizer.qml"))
       // console.log(contentRoot["cms_root"]);
    }
}



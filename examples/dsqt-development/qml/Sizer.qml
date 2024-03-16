import QtQuick
import QtMultimedia
Item {
    id:sizer
    Component.onCompleted: {
        console.log("Sizer size:"+sizer.width+","+sizer.height)
    }

    Rectangle {
        id:baseRect
        anchors.fill: parent
        color: "#001080"
    }

    Rectangle {
        x:baseRect.x+20;
        y:baseRect.y+20;
        width:baseRect.width-40;
        height:baseRect.height-40;
        color: "#ff1080"
    }

    Rectangle {
        y:sizer.height*0.5;
        x:0
        width: sizer.width;
        height: 20;
        color: "#000000"
    }

    Repeater {
        id: repline
        model: sizer.width/150.0
        Text {
            y:sizer.height*0.5+20;
            x:modelData*150
            width:contentWidth
            height:contentHeight
            text:""+modelData*150
            font.pixelSize: 50
            color: "#000000"
        }
    }

    // SequentialAnimation on x {
    //     id: anim
    //     property int initScale;
    //     loops: Animation.Infinite
    //     NumberAnimation { to: sizer.width; duration: 10000 * initScale; onStopped: { anim.initScale=1; }}
    //     NumberAnimation { to: -sizer.width; duration: 10000 }
    //     Component.onCompleted: {
    //         initScale = 1.0 - (x-(-sizer.width)/(sizer.width - (-sizer.width)));
    //         console.log(initScale)
    //     }
    // }


        VideoOutput {
            id: output
            anchors.fill: parent
        }

        CaptureSession {
            videoOutput: output

            camera: Camera {
                id: camera
                active: true
                cameraDevice: CameraDevice.getCamera(0)
                // You can adjust various settings in here
            }
        }


}

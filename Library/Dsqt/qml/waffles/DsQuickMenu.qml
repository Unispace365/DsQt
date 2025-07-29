pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes
import QtQuick.Effects
import Dsqt

Item {
    id: root
    property Component delegate;
    property list<var> model;
    property var config;
    property real startAngle: 0;
    property real innerRadius: 250*0.5;
    property real outerRadius: 460*0.5;
    property real cInnerRadius: innerRadius;
    property real cOuterRadius: outerRadius;
    property string spacing: "equal" //fixed, equal
    property string style: "spoke" // spoke, curve, circle
    property real spacingValue: 20;
    property color highlightColor: "#00ADF7"
    property int selection: 0
    property point clusterPoint
    property bool fullyOpen: false
    property int closeTimer: appSettings.getInt("quickMenu.closeDelay") ?? 5000 //in ms, how long to wait before closing the menu after a click
    signal itemHighlighted(index: int, highlight: bool)
    signal itemSelected(index: int)

    state: "Off"

    width: cOuterRadius * 2+2;
    height: cOuterRadius * 2+2;

    transform: Translate {
        x: -root.width*0.5
        y: -root.height*0.5
    }

    DsSettingsProxy {
        id:appSettings
        target:"app_settings"
    }

    Component.onCompleted: {
        console.log("IconPath: "+config.iconPath)
    }

    //the main menu drawing
    Item {
        enabled: false
        id: menubase
        width: root.cOuterRadius * 2+4
        height: root.cOuterRadius * 2+4
        visible: false

    //this rectangle is the background circle.
    Rectangle {
            id:background
            color: "#191F25"
            opacity: 0.8
            anchors.horizontalCenter: menubase.horizontalCenter
            anchors.verticalCenter: menubase.verticalCenter
            width: root.cOuterRadius * 2
            height: root.cOuterRadius * 2
            radius: root.cOuterRadius
        }


        Shape {
            anchors.horizontalCenter: menubase.horizontalCenter
            anchors.verticalCenter: menubase.verticalCenter
            width: root.cOuterRadius * 2;
            height: root.cOuterRadius * 2;
            preferredRendererType: Shape.CurveRenderer;
            ShapePath {
                strokeColor: "#2E3439"
                strokeWidth: 4
                fillColor: "Transparent"
                startX: root.cOuterRadius
                startY: 2
                PathAngleArc {
                    centerX: root.cOuterRadius; centerY: root.cOuterRadius
                    radiusX: root.cOuterRadius; radiusY: root.cOuterRadius
                    startAngle: 0
                    sweepAngle: 360
                    moveToStart: true
                }
                PathMove{
                    x: root.cOuterRadius; y: root.cOuterRadius - (root.cInnerRadius+2);
                }

                PathAngleArc {
                    centerX: root.cOuterRadius; centerY: root.cOuterRadius
                    radiusX: root.cInnerRadius+2; radiusY: root.cInnerRadius+2
                    startAngle: 0
                    sweepAngle: 360
                    moveToStart: true
                }

            }
        }

        Repeater {
            id:lineRepeater
            model: root.model
            anchors.horizontalCenter: menubase.horizontalCenter
            anchors.verticalCenter: menubase.verticalCenter
            Shape {
                id: pielines
                required property int index
                required property var modelData
                anchors.horizontalCenter: lineRepeater.horizontalCenter
                anchors.verticalCenter: lineRepeater.verticalCenter
                width: root.cOuterRadius * 2;
                height: root.cOuterRadius * 2;
                preferredRendererType: Shape.CurveRenderer;
                transform: Rotation {
                    origin.x: root.cOuterRadius;
                    origin.y: root.cOuterRadius;
                    angle: 360.0/lineRepeater.count * pielines.index;
                }

                ShapePath {
                    strokeColor: "#2E3439"
                    strokeWidth: 4
                    fillColor: "Transparent"
                    startX: root.cOuterRadius
                    startY: root.cOuterRadius

                    PathLine {
                        x: root.cOuterRadius-1
                        y: 0
                    }
                }


            }
        }
    }

    //the center mask shape
    Shape {
        enabled: false
        id: centerMask;
        visible:false
        layer.enabled: true;
        layer.live: root.state != "On" || toOnTransition.running || toOffTransition.running
        anchors.horizontalCenter: menubase.horizontalCenter
        anchors.verticalCenter: menubase.verticalCenter
        width: root.cOuterRadius * 2;
        height: root.cOuterRadius * 2;
        preferredRendererType: Shape.CurveRenderer;
        ShapePath {
            strokeColor: "transparent"
            strokeWidth: 4
            fillColor: "#ffffff"
            startX: root.cOuterRadius
            startY: 0



            PathAngleArc {
                centerX: root.cOuterRadius; centerY: root.cOuterRadius
                radiusX: root.cInnerRadius-1; radiusY: root.cInnerRadius-1
                startAngle: 0
                sweepAngle: 360
                moveToStart: true
            }
        }
    }

    //do the cutout
    MultiEffect {
        source: menubase

        width: menubase.width
        height: menubase.height
        anchors.horizontalCenter: menubase.horizontalCenter
        anchors.verticalCenter: menubase.verticalCenter
        maskEnabled: true;
        maskSource: centerMask;
        maskInverted: true;
    }

    //selection lines
    Item {
        enabled: false
        id: selected

        property int selection: 0
        property int lineWidth: 4
        width: root.cOuterRadius * 2 + lineWidth*2
        height: root.cOuterRadius * 2 + lineWidth*2
        anchors.horizontalCenter: menubase.horizontalCenter
        anchors.verticalCenter: menubase.verticalCenter
        Repeater {
            id:highlightRep
            model: root.model
            anchors.horizontalCenter: selected.horizontalCenter
            anchors.verticalCenter: selected.verticalCenter
            onItemAdded: (index,item) => {

            }
            Shape {
                id:highlight
                required property int index
                required property var modelData
                opacity: (root.selection >> highlight.index) & 1 ? 1 : 0
                anchors.horizontalCenter: highlightRep.horizontalCenter
                anchors.verticalCenter: highlightRep.verticalCenter
                width: root.cOuterRadius * 2;
                height: root.cOuterRadius * 2;
                preferredRendererType: Shape.CurveRenderer;
                transform: Rotation {
                    origin.x: root.cOuterRadius;
                    origin.y: root.cOuterRadius;
                    angle: 360.0/highlightRep.count * highlight.index;
                }

                ShapePath {
                    strokeColor: root.highlightColor
                    strokeWidth: selected.lineWidth
                    fillColor: "Transparent"
                    startX: root.cOuterRadius
                    startY: root.cOuterRadius

                    PathAngleArc {
                        centerX: root.cOuterRadius
                        centerY: root.cOuterRadius
                        radiusX: root.cOuterRadius-selected.lineWidth*0.5
                        radiusY: root.cOuterRadius-selected.lineWidth*0.5
                        sweepAngle: 360.0/highlightRep.count
                        startAngle: -90
                        moveToStart: true
                    }

                    PathAngleArc {
                        centerX: root.cOuterRadius
                        centerY: root.cOuterRadius
                        radiusX: root.cInnerRadius+selected.lineWidth*0.5
                        radiusY: root.cInnerRadius+selected.lineWidth*0.5
                        sweepAngle: 360.0/highlightRep.count
                        startAngle: -90
                        moveToStart: true
                    }

                }

                Behavior on opacity {
                    NumberAnimation {
                        duration: 200
                    }
                }
            }
        }
    }

    //the blue selection arrow
    Repeater {
        id:arrowRep
        model: root.model
        anchors.horizontalCenter: root.horizontalCenter
        anchors.verticalCenter: root.verticalCenter
        width: root.width;
        height: root.height
        Item {
            id: arrow
            width: root.width
            height: root.height
            opacity: (root.selection >> arrow.index) & 1 ? 1 : 0

            required property int index
            required property var modelData

            transform: Rotation {
                origin.x: root.cOuterRadius;
                origin.y: root.cOuterRadius;
                angle: 360.0/arrowRep.count * arrow.index + 360.0/arrowRep.count*0.5;
            }
            Image {
                id: arrowSvg
                //source: "qrc:/res/data/waffles/ui/quickMenuSelectionArrow"
                source: DS.env.expand("file:///%APP%/data/images/waffles/quick_menu/quickMenuSelectionArrowwt.svg")
                visible: false
                width: 24; height: 24;
                smooth: true
                x: root.cOuterRadius - width*0.5;
                y: (root.cOuterRadius - root.cInnerRadius*0.9);

            }
            MultiEffect {
                anchors.fill: arrowSvg
                source: arrowSvg
                colorization: 1.0
                colorizationColor: "#00ADF7"
            }

        }
    }

    //the glow of the selection lines
    Item {
        enabled: false
        id: selectedBlur
        layer.enabled: true
        visible: false
        opacity: 0
        width: root.cOuterRadius * 2 + selectedBlur.blurlineWidth
        height: root.cOuterRadius * 2 + selectedBlur.blurlineWidth
        anchors.horizontalCenter: menubase.horizontalCenter
        anchors.verticalCenter: menubase.verticalCenter
        property int blurlineWidth: 16
        Repeater {
            id:highlightBlurRep
            model: root.model
            anchors.horizontalCenter: selectedBlur.horizontalCenter
            anchors.verticalCenter: selectedBlur.verticalCenter
            onItemAdded: (index,item) => {

            }
            Shape {
                id:highlightBlur
                required property int index
                required property var modelData
                opacity: (root.selection >> highlightBlur.index) & 1 ?1 : 0
                anchors.horizontalCenter: highlightBlurRep.horizontalCenter
                anchors.verticalCenter: highlightBlurRep.verticalCenter
                width: root.cOuterRadius * 2;
                height: root.cOuterRadius * 2;
                transform: Rotation {
                    origin.x: root.cOuterRadius;
                    origin.y: root.cOuterRadius;
                    angle: 360.0/highlightBlurRep.count * highlightBlur.index;
                }

                ShapePath {
                    strokeColor: root.highlightColor
                    strokeWidth: 8
                    fillColor: "Transparent"
                    startX: root.cOuterRadius
                    startY: root.cOuterRadius

                    PathAngleArc {
                        centerX: root.cOuterRadius
                        centerY: root.cOuterRadius
                        radiusX: root.cOuterRadius-selectedBlur.blurlineWidth*0.25;
                        radiusY: root.cOuterRadius-selectedBlur.blurlineWidth*0.25;
                        sweepAngle: 360.0/highlightBlurRep.count
                        startAngle: -90
                        moveToStart: true
                    }

                    PathAngleArc {
                        centerX: root.cOuterRadius
                        centerY: root.cOuterRadius
                        radiusX: root.cInnerRadius+selectedBlur.blurlineWidth*0.25;
                        radiusY: root.cInnerRadius+selectedBlur.blurlineWidth*0.25;
                        sweepAngle: 360.0/highlightBlurRep.count
                        startAngle: -90
                        moveToStart: true
                    }
                }

                Behavior on opacity {
                    NumberAnimation {
                        duration: 100
                    }
                }
            }
        }
    }

    //the icons
    Item {
        id:iconLoop
        width: root.cOuterRadius * 2
        height: root.cOuterRadius * 2
        anchors.horizontalCenter: menubase.horizontalCenter
        anchors.verticalCenter: menubase.verticalCenter

        state: root.fullyOpen ? "On" : "Off"
        states: [
            State {
                name: "Start"
                PropertyChanges { iconLoop.opacity:0}

            },
            State {
                name: "Off"
                PropertyChanges { iconLoop.opacity:0}

            },
            State {
                name: "On"
                PropertyChanges { iconLoop.opacity: 1}
            }
        ]
        transitions: [
            Transition {
                from: "Off,Start"
                to: "On"
                NumberAnimation { properties: "opacity"; duration: 250 }


            },
            Transition {
                from: "On"
                to: "Off"
                NumberAnimation { properties: "opacity"; duration: 0 }
            }
        ]

        Repeater {
            id:iconRep
            model: root.model
            anchors.horizontalCenter: iconLoop.horizontalCenter
            anchors.verticalCenter: iconLoop.verticalCenter
            property real outerRadius: root.cOuterRadius;
            onItemAdded: (index,item) => {
                let x = item.icon.x
                let y = item.icon.y
                let sliceSize = 360.0/iconRep.count;
                let out = root.rotate(root.cOuterRadius,root.cOuterRadius,x,y,sliceSize*index+sliceSize*0.5)
                item.icon.x = out[0]
                item.icon.y = out[1]
            }

            Item {
                id: iconSet
                anchors.horizontalCenter: iconRep.horizontalCenter
                anchors.verticalCenter: iconRep.verticalCenter
                width: root.cOuterRadius * 2;
                height: root.cOuterRadius * 2;
                required property int index
                required property var modelData
                property alias icon: icon
                transform: [Translate {
                    x: (iconSet.modelData?.iconWidth ?? icon.width) * -0.5
                    y: (iconSet.modelData?.iconHeight ?? icon.height) * -0.5
                }]
                Image {
                    id: icon
                    source: Qt.url(
                            DS.env.expand(
                                    (iconSet.modelData.iconPath ? iconSet.modelData.iconPath ?? "" : root.config.iconPath ?? "")
                                    + iconSet.modelData?.icon ?? ""))
                    fillMode: Image.PreserveAspectFit

                    x: root.cOuterRadius;
                    y: (root.cOuterRadius-root.cInnerRadius) * 0.5
                    width: iconSet.modelData?.iconWidth ?? icon.implicitWidth
                    height: iconSet.modelData?.iconHeight ?? icon.implicitHeight

                }


                Text {
                    id: iconText
                    color: "#bbbbbb"
                    font.pixelSize: root.config.fontSize ?? 8
                    font.family: root.config.fontFamily ?? "Helvetica Nueue"
                    font.weight: root.config.fontWeight ?? 400
                    horizontalAlignment: Text.AlignHCenter
                    text: iconSet.modelData?.iconText ?? ""
                    anchors.horizontalCenter: icon.horizontalCenter
                    anchors.verticalCenter: iconSet.modelData?.iconPath ? undefined : icon.verticalCenter
                    anchors.top: iconSet.modelData?.icon ? icon.bottom : undefined
                }
            }
        }
    }

    MultiEffect {
        id: blurr
        visible: true
        source: selectedBlur
        anchors.horizontalCenter: selectedBlur.horizontalCenter
        anchors.verticalCenter: selectedBlur.verticalCenter
        width:selectedBlur.width
        height:selectedBlur.height
        blurEnabled: true;
        blur: 1
        blurMax: 32
        blurMultiplier: 1.5 + (root.selection*0.0001)
    }

    //button outlines.
    Item {
        enabled: true
        id: buttons
        visible: root.state == "On"// && toOnTransition.running == false;
        width: root.cOuterRadius * 2
        height: root.cOuterRadius * 2
        anchors.horizontalCenter: menubase.horizontalCenter
        anchors.verticalCenter: menubase.verticalCenter
        property bool transitionRunning: toOnTransition.running
        property int blurlineWidth: 16
        property var basepoints: [{'x':root.cOuterRadius,'y':0},{'x':0,'y':0},{'x':0,'y':0},{'x':0,'y':0},{'x':0,'y':0},{'x':root.cOuterRadius,'y':root.cOuterRadius-root.cInnerRadius}]
        property var points: [{'x':root.cOuterRadius,'y':0},{'x':0,'y':0},{'x':0,'y':0},{'x':0,'y':0},{'x':0,'y':0},{'x':root.cOuterRadius,'y':root.cOuterRadius-root.cInnerRadius}]
        Repeater {
            id:buttonRep
            model: root.model
            anchors.horizontalCenter: buttons.horizontalCenter
            anchors.verticalCenter: buttons.verticalCenter

            Shape {
                id:button
                required property int index
                required property var modelData
                anchors.horizontalCenter: buttonRep.horizontalCenter
                anchors.verticalCenter: buttonRep.verticalCenter
                width: root.cOuterRadius * 2;
                height: root.cOuterRadius * 2;
                containsMode: Shape.FillContains
                property point clusterPoint: root.clusterPoint
                transform: Rotation {
                   origin.x: root.cOuterRadius;
                   origin.y: root.cOuterRadius;
                   angle: 360.0/buttonRep.count * button.index;
                }

                ShapePath {
                    strokeColor: ((root.selection >> button.index) & 1) * 0? "#ff0000" : "transparent"
                    strokeWidth: 1
                    fillColor: "transparent"
                    startX: root.cOuterRadius
                    startY: root.cOuterRadius

                    PathMove {
                        x: buttons.points[0].x
                        y: buttons.points[0].y
                    }

                    PathLine {
                        x: buttons.points[1].x
                        y: buttons.points[1].y
                    }

                    PathLine {
                        x: buttons.points[2].x
                        y: buttons.points[2].y
                    }

                    PathLine {
                        x: buttons.points[3].x
                        y: buttons.points[3].y
                    }

                    PathLine {
                        x: buttons.points[4].x
                        y: buttons.points[4].y
                    }

                    PathLine {
                        x: buttons.points[5].x
                        y: buttons.points[5].y
                    }

                    PathLine {
                        x: buttons.points[0].x
                        y: buttons.points[0].y
                    }

                }

                //handle mouse click, and only mouse clicks, touches are
                //handled below.
                TapHandler {
                    id: buttonTap
                    acceptedPointerTypes: PointerDevice.Mouse
                    onPressedChanged: {
                        if(pressed){
                            root.selection |= (1<<button.index);
                            root.itemHighlighted(button.index,true);
                        } else {
                            root.selection &= ~(1<<button.index);
                            root.itemHighlighted(button.index,false);
                        }
                    }
                    onTapped: {
                        root.state = "Off"
                        root.selection = 0
                        root.itemSelected(button.index);
                    }
                }
            }
        }

        function updateOutline() {
            //calculate the position.
            let nPoint = buttons.basepoints;

            for(let i=1;i<3;i++){
                let pos = root.rotate(root.cOuterRadius,root.cOuterRadius,nPoint[i-1].x,nPoint[i-1].y,360.0/buttonRep.count * 0.5);
                nPoint[i].x = pos[0]; nPoint[i].y = pos[1];
                let r = 3-i;
                pos = root.rotate(root.cOuterRadius,root.cOuterRadius,nPoint[r+3].x,nPoint[r+3].y,360.0/buttonRep.count * 0.5);
                nPoint[r+2].x = pos[0]; nPoint[r+2].y = pos[1];
            }
            buttons.points = nPoint
        }

        onTransitionRunningChanged: {
            updateOutline()
        }


        Component.onCompleted: {
           updateOutline()
        }
    }

    Item {
        id:closeBtn
        width: childrenRect.width;
        height: childrenRect.height;
        x: root.cOuterRadius;
        y: root.cOuterRadius;
        transform: Translate {
            x: -closeBtn.width*0.5;
            y: -closeBtn.height*0.5;
        }

        Image {
            id: closeUp
            visible: !closeTapHandler.pressed
            source: DS.env.expandUrl("file:///%APP%/data/images/waffles/quick_menu/close.svg");
            fillMode: Image.PreserveAspectFit
            width: closeUp.implicitWidth
            height: closeUp.implicitHeight

        }

        Image {
            id: closeDwn
            visible: closeTapHandler.pressed
            source: DS.env.expandUrl("file:///%APP%/data/images/waffles/quick_menu/close_pressed.svg");
            fillMode: Image.PreserveAspectFit
            width: closeDwn.implicitWidth
            height: closeDwn.implicitHeight
        }

        TapHandler{
            id:closeTapHandler

            onTapped: {
                root.state = "Off"
                root.selection = 0
            }
        }
    }

    function rotate(cx, cy, x, y, angle) {
        var radians = (Math.PI / 180) * angle,
            cos = Math.cos(radians),
            sin = -Math.sin(radians), //clockwise
            nx = (cos * (x - cx)) + (sin * (y - cy)) + cx,
            ny = (cos * (y - cy)) - (sin * (x - cx)) + cy;
        return [nx, ny];
    }

    //update the cluserPoint for this menu. The point is coming from
    //the ClusterView (via ClusterView.onUpdated see below)
    //this only handles touches (and sudo touches via shift-mouseclick),
    //Not normal mouse clicks. Those are handled via a tapHandler
    function updatePoint(point) {
        root.clusterPoint = point
        let x1 = point.x - root.width*0.5
        let y1 = point.y - root.height*0.5
        let len = Math.sqrt(x1*x1+y1*y1);

        if(len>root.innerRadius && len < root.outerRadius ){
            let segRads = 360/root.model.length;
            for(let seg=0;seg<root.model.length;seg++){
                let startAngle =  segRads * seg;
                let endAngle = segRads * (seg+1);
                let angle = Math.atan2(-y1,-x1)*(180/Math.PI)+180-270
                if(angle<0) angle+=360;
                if(startAngle<angle && angle<endAngle) {
                    root.selection |= (1<<seg);
                    root.itemHighlighted(seg,true);
                } else {
                    root.selection &= ~(1<<seg);
                    root.itemHighlighted(seg,false);
                }
            }
        } else {
            root.selection = 0;
        }
    }



    /*Rectangle {
            id:tester_d
            color: "blue"
            opacity: 1.0
            x: root.clusterPoint.x
            y: root.clusterPoint.y
            width: 20
            height: 20
        }
    */

    Timer {
        id: closeTimer
        interval: root.closeTimer
        repeat: false
        running: false
        onTriggered: {
            root.state = "Off"
            root.selection = 0
        }
    }

    DsClusterView.onMinimumMetChanged: {
        if(DsClusterView.minimumMet){
            root.state = "On"
        }
    }

    DsClusterView.onRemoved: {
        root.state = "Off"
    }

    DsClusterView.onReleased: {
        for(let seg=0;seg<root.model.length;seg++){
            if((root.selection >> seg) & 1){
                console.log("Selected segment "+seg);
                root.state = "Off"
                root.selection = 0
                root.itemSelected(seg);
                break;
            }
        }
        if(root.state != "Off"){
            closeTimer.running = true;
        }
    }

    DsClusterView.onUpdated: (point)=>{
        updatePoint(point);
    }




    //ClusterView.onUpdated: (point)=>{
    //    root.clusterPoint = point;
    //}

    states: [
        State {
            name: "Start"
            PropertyChanges { root.opacity:0}
            PropertyChanges { root.cInnerRadius: 0 }
            PropertyChanges { root.cOuterRadius: 0 }

        },
        State {
            name: "Off"
            PropertyChanges { root.opacity:0}
            PropertyChanges { root.cInnerRadius: 0 }
            PropertyChanges { root.cOuterRadius: 0 }
        },
        State {
            name: "On"
            PropertyChanges { root.opacity: 1}
            PropertyChanges { root.visible: true}
            PropertyChanges { root.cInnerRadius: root.innerRadius }
            PropertyChanges { root.cOuterRadius: root.outerRadius }
        }

    ]
    transitions: [
        Transition {
            from: "Off,Start"
            to: "On"
            id: toOnTransition
            SequentialAnimation {
                SmoothedAnimation { properties: "opacity,cInnerRadius,cOuterRadius"; duration: 100 }
                ScriptAction {
                    script: {
                        root.fullyOpen = true;
                        root.selection = 0
                    }
                }
            }
        },
        Transition {
            from: "On"
            to: "Off"
            id: toOffTransition
            SequentialAnimation {
                ScriptAction {
                    script: root.fullyOpen = false
                }
                ParallelAnimation {
                    NumberAnimation {
                    target: root
                    property: "opacity"
                    duration: 200
                    }
                    SmoothedAnimation { properties: "cInnerRadius,cOuterRadius"; duration: 250}
                }
                PropertyAction { target: root; property: "visible"; value:false }
                ScriptAction {
                    script: root.ClusterView.animateOffFinished()
                }
            }
        }
    ]
}


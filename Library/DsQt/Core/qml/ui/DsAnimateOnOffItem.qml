import QtQuick

//@brief An item that provides animateOn and animateOff signals and runs the associated animations.
//the provided animateOnAnimation runs on Component.onCompleted.
// It is up to the user to call runAnimateOff() when desired.
Item {
    required property Animation animateOnAnimation
    required property Animation animateOffAnimation
    Component.onCompleted: {
        runAnimateOn()
    }

    function runAnimateOn() {
        console.log("animate on");
        animateOn();
        if(animateOnAnimation) {
            animateOnAnimation.start();
        }
    }

    function runAnimateOff() {
        console.log("animate off");
        animateOff();
        if(animateOffAnimation) {
            animateOffAnimation.start();
        }
    }

    signal animateOn()
    signal animateOff()
}

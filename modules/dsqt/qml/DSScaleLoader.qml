import QtQuick
import dsqt



Loader {
   id: loadera
   property alias scale_origin: loaderScale.origin
   property alias xScale: loaderScale.xScale;
   property alias yScale: loaderScale.yScale;
   property alias xTrans: loaderTrans.x;
   property alias yTrans: loaderTrans.y;

   width: windowProxy.getSize("world_dimensions").width;
   height: windowProxy.getSize("world_dimensions").height;

   transform: [
        Scale {
           id: loaderScale
           origin.x:0
           origin.y:0
           xScale: 1;
           yScale: 1;
        },
        Translate {
              id: loaderTrans
        }
   ]
   function reload() {
               source = "";
               $QmlEngine.clearQmlCache();
               source = rootSrc;
           }
   Component.onCompleted: {
       //resize the loader to fit in the destination
       var src = windowProxy.getRect("source");
       var dst = Qt.size(window.width,window.height);
       var x_scale = dst.width/src.width
       var y_scale = dst.height/src.height
       var offset = Qt.point(src.x+0.5*src.width,src.y+0.5*src.height);
       console.log("scale: "+x_scale+","+y_scale);
       console.log("offset "+offset.x+","+offset.y);

       loadera.x = -offset.x + 0.5*dst.width;
       loadera.y = -offset.y + 0.5*dst.height;
       loadera.scale_origin.x = offset.x;
       loadera.scale_origin.y = offset.y;
       loadera.xScale = x_scale;
       loadera.yScale = y_scale;
       //loader.xTrans = -offset.x + 0.5*dst.width;
       //loader.yTrans = -offset.y + 0.5*dst.height;

   }

}

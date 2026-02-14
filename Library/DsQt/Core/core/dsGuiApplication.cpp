#include "core/dsGuiApplication.h"
#include <QQuickWindow>

DsGuiApplication::DsGuiApplication(int &argc, char **argv):QGuiApplication(argc,argv)
{
}

void DsGuiApplication::configureGraphics(const DsGraphicsConfig& config)
{
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();

    int depth = (config.colorDepth == 10) ? 10 : 8;
    format.setRedBufferSize(depth);
    format.setGreenBufferSize(depth);
    format.setBlueBufferSize(depth);
    format.setAlphaBufferSize(depth);

    if (config.samples > 0) {
        format.setSamples(config.samples);
    }

    format.setSwapBehavior(config.swapBehavior);
    format.setSwapInterval(config.swapInterval);

    QSurfaceFormat::setDefaultFormat(format);

    if (config.graphicsApi.has_value()) {
        QQuickWindow::setGraphicsApi(config.graphicsApi.value());
    }
}

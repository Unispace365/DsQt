#include "core/dsGuiApplication.h"
#include <QQuickWindow>

DsGuiApplication::DsGuiApplication(int &argc, char **argv):QGuiApplication(argc,argv)
{
}

void DsGuiApplication::configureGraphics(const DsGraphicsConfig& config)
{
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();

    // Skip 10-bit surface format on OpenGL — NVIDIA's 10-bit WGL pixel
    // formats use a linear gamma ramp that washes out sRGB content.
    // Dithering in the Spout shader handles banding on 8-bit targets instead.
    bool isOpenGL = config.graphicsApi.has_value()
        && config.graphicsApi.value() == QSGRendererInterface::OpenGL;
    int depth = (config.colorDepth == 10 && !isOpenGL) ? 10 : 8;
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

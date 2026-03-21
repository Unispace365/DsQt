
#ifndef DSGUIAPPLICATION_H
#define DSGUIAPPLICATION_H

#include <optional>
#include <QGuiApplication>
#include <QSurfaceFormat>
#include <QSGRendererInterface>

struct DsGraphicsConfig {
    // Graphics API (nullopt = Qt default)
    std::optional<QSGRendererInterface::GraphicsApi> graphicsApi;

    // Color depth per channel: 8 (default) or 10
    int colorDepth = 8;

    // Swap behavior (double/triple buffering)
    QSurfaceFormat::SwapBehavior swapBehavior = QSurfaceFormat::DefaultSwapBehavior;

    // MSAA samples (0 = off, 2, 4, 8)
    int samples = 0;

    // VSync: swap interval (1 = vsync on, 0 = off)
    int swapInterval = 1;
};

class DsGuiApplication : public QGuiApplication
{
  public:
    DsGuiApplication(int &argc, char **argv);

    static void configureGraphics(const DsGraphicsConfig& config = {});

  private:
    void initializeLogging();
    void printStartupBanner();
};
#endif // DSGUIAPPLICATION_H

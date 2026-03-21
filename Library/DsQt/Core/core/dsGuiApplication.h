
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

    // Resolve an INI path value to a full log file path.
    // If path is empty, returns defaultLogPath. If path has no file extension
    // (or is an existing directory), appends defaultLogName.
    static QString resolveLogPath(const QString &path, const QString &defaultLogName,
                                  const QString &defaultLogPath);

  private:
    void initializeLogging();
    void printStartupBanner();
};
#endif // DSGUIAPPLICATION_H

#include "core/dsGuiApplication.h"
#include "core/dsEnvironment.h"
#include "core/dsVersion.h"
#define QTLOGGER_STATIC
#include "utility/qtlogger.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QQuickWindow>
#include <QSettings>
#include <QStandardPaths>

DsGuiApplication::DsGuiApplication(int &argc, char **argv):QGuiApplication(argc,argv)
{
    initializeLogging();
}

void DsGuiApplication::initializeLogging()
{
    const QString appName = applicationName().isEmpty()
                            ? QFileInfo(applicationFilePath()).baseName()
                            : applicationName();
    const QString defaultLogName = appName + QStringLiteral(".log.txt");
    const QString defaultLogDir  = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                                   + QStringLiteral("/downstream/logs/") + appName;
    const QString defaultLogPath = defaultLogDir + QStringLiteral("/") + defaultLogName;

    const QString iniPath = DsEnv::expandq(QStringLiteral("%APP%/settings/logging.ini"));

    auto *logger = QtLogger::Logger::instance();

    if (QFileInfo::exists(iniPath)) {
        QSettings settings(iniPath, QSettings::IniFormat);
        // Allow the INI to override the log file path; fall back to default.
        // The path value can be a directory or a full file path.
        QString rawPath = settings.value(QStringLiteral("logger/path")).toString();
        QString expandedPath = rawPath;
        bool removePath = !settings.contains("logger/path");
        if (!rawPath.isEmpty()){
            expandedPath = DsEnv::expandq(rawPath);
        }
        QString logPath = resolveLogPath(expandedPath, defaultLogName, defaultLogPath);
        settings.setValue(QStringLiteral("logger/path"), logPath);
        QDir().mkpath(QFileInfo(logPath).absolutePath());

        logger->configure(settings, QStringLiteral("logger"));
        if(removePath){
            settings.remove("logger/path");

        } else {
            settings.setValue(QStringLiteral("logger/path"), rawPath);

        }
        settings.sync();
    } else {
        QDir().mkpath(defaultLogDir);
        logger->configure(defaultLogPath, 1048576, 5,
                          QtLogger::RotatingFileSink::RotationOnStartup, true);
    }

    printStartupBanner();
}

void DsGuiApplication::printStartupBanner()
{
    qInfo() << "========================================";
    qInfo() << applicationName() << "v" << applicationVersion();
    qInfo() << "DsQt" << DSQT_VERSION << "| Qt" << qVersion() << "| PID:" << applicationPid();
    qInfo() << "Started:" << QDateTime::currentDateTime().toString(Qt::ISODate);
    qInfo() << "========================================";
}

QString DsGuiApplication::resolveLogPath(const QString &path, const QString &defaultLogName,
                                         const QString &defaultLogPath)
{
    if (path.isEmpty())
        return defaultLogPath;

    QFileInfo fi(path);
    if (fi.suffix().isEmpty() || fi.isDir())
        return path + QStringLiteral("/") + defaultLogName;

    return path;
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

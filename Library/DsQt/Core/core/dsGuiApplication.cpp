#include "core/dsGuiApplication.h"
#include "core/dsEnvironment.h"
#include "core/dsVersion.h"
#define QTLOGGER_STATIC
#include "utility/qtlogger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QQuickWindow>
#include <QSettings>
#include <QStandardPaths>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace {

#ifdef Q_OS_WIN
// Use a single destination even when the debugger captures both console and
// debug events. Checking per message also handles attaching/detaching later.
class WindowsConsoleSink : public QtLogger::Sink
{
public:
    explicit WindowsConsoleSink(QtLogger::SinkPtr console) : m_console(std::move(console)) {}

    void send(const QtLogger::LogMessage &message) override
    {
        if (IsDebuggerPresent())
            m_debug.send(message);
        else
            m_console->send(message);
    }

    bool flush() override { return m_console->flush(); }

private:
    QtLogger::SinkPtr m_console;
    QtLogger::WinDebugSink m_debug;
};
#endif

void routePlatformConsole(const QtLogger::SimplePipelinePtr &pipeline, bool platformEnabled)
{
#ifdef Q_OS_WIN
    if (!platformEnabled)
        return;

    // QtLogger appends the platform sink after any explicit stderr sink. Replace
    // only that last platform sink, preserving formatter/filter/sink order.
    const auto handlers = static_cast<const QtLogger::Pipeline &>(*pipeline).handlers();
    QtLogger::HandlerPtr platform;
    for (const auto &handler : handlers) {
        if (handler.dynamicCast<QtLogger::PlatformStdSink>())
            platform = handler;
    }
    pipeline->clear();
    for (const auto &handler : handlers) {
        if (handler == platform)
            pipeline->append(QSharedPointer<WindowsConsoleSink>::create(platform.dynamicCast<QtLogger::Sink>()));
        else
            pipeline->append(handler);
    }
#else
    Q_UNUSED(pipeline)
    Q_UNUSED(platformEnabled)
#endif
}

QString prepareLogFile(const QString &path)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return QStringLiteral("Cannot create log directory for %1").arg(path);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append))
        return QStringLiteral("Cannot open log file %1: %2").arg(path, file.errorString());
    return {};
}

} // namespace

DsGuiApplication::DsGuiApplication(int &argc, char **argv):QApplication(argc,argv)
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
    // Build the pipeline before starting its worker or installing the message
    // handler, so configuration errors cannot feed back into a failing file sink.
    const auto pipeline = QtLogger::SimplePipelinePtr::create();
    bool async = true;
    bool platformEnabled = true;
    QString fileError;

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
        fileError = prepareLogFile(logPath);
        m_logPath = fileError.isEmpty() ? logPath : QString();
        settings.setValue(QStringLiteral("logger/path"), m_logPath);
        async = settings.value(QStringLiteral("logger/async"), false).toBool();
        platformEnabled = settings.value(QStringLiteral("logger/platform_std_log"), true).toBool();
        QtLogger::configure(pipeline.data(), settings, QStringLiteral("logger"));
        if (!fileError.isEmpty() && !platformEnabled
            && !settings.value(QStringLiteral("logger/stdout"), false).toBool()
            && !settings.value(QStringLiteral("logger/stdout_color"), false).toBool()
            && !settings.value(QStringLiteral("logger/stderr"), false).toBool()
            && !settings.value(QStringLiteral("logger/stderr_color"), false).toBool()) {
            pipeline->sendToPlatformStdLog();
            platformEnabled = true;
        }
        if(removePath){
            settings.remove("logger/path");

        } else {
            settings.setValue(QStringLiteral("logger/path"), rawPath);

        }
        settings.sync();
    } else {
        fileError = prepareLogFile(defaultLogPath);
        m_logPath = fileError.isEmpty() ? defaultLogPath : QString();
        QtLogger::configure(pipeline.data(), m_logPath, 1048576, 5,
                            QtLogger::RotatingFileSink::RotationOnStartup, false);
    }

    routePlatformConsole(pipeline, platformEnabled);
    logger->append(pipeline);
    if (async)
        logger->moveToOwnThread();
    logger->installMessageHandler();

    if (!fileError.isEmpty())
        qWarning().noquote() << "DsQt file logging disabled:" << fileError;

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

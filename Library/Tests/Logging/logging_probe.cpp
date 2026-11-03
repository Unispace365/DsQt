#include "core/dsGuiApplication.h"
#include "core/dsEnvironment.h"

#include <QLoggingCategory>
#include <QTimer>

Q_LOGGING_CATEGORY(probeCategory, "dsqt.logging.probe")
Q_LOGGING_CATEGORY(probeVerbose, "dsqt.logging.probe.verbose")

int main(int argc, char **argv)
{
    DsGuiApplication app(argc, argv);
    dsqt::DsEnvironment::loadEngineSettings();
    qInfo("DSQT_PROBE_VISIBLE");
    qCInfo(probeCategory, "DSQT_PROBE_CATEGORY");
    qCInfo(probeVerbose, "DSQT_PROBE_VERBOSE");
    qInfo().noquote() << "DSQT_PROBE_LOG_PATH" << app.getLogPath();
    // Exercise asynchronous output and leave time for the IDE to attach its reader.
    QTimer::singleShot(250, &app, &QCoreApplication::quit);
    return app.exec();
}

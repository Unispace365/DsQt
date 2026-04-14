#include "core/dsQmlEnvironment.h"
#include "core/dsQmlApplicationEngine.h"
#include "core/dsEnvironment.h"
#include "core/dsGuiApplication.h"

namespace dsqt {

DsQmlEnvironment::DsQmlEnvironment(QObject* parent)
    : QObject{parent} {

}

const QString dsqt::DsQmlEnvironment::expand(const QString& string) {
    const std::string val = DsEnvironment::expand(string.toStdString());
    return QString::fromStdString(val);
}

const QUrl DsQmlEnvironment::expandUrl(const QString& string) {
    return QUrl(expand(string));
}

const QString DsQmlEnvironment::logFile()
{
    DsGuiApplication* app = dynamic_cast<DsGuiApplication*>(QCoreApplication::instance());
    return app->getLogPath();
}



} // namespace dsqt

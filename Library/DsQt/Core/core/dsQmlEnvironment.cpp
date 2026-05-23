#include "core/dsQmlEnvironment.h"
#include "core/dsQmlApplicationEngine.h"
#include "core/dsEnvironment.h"
#include "core/dsGuiApplication.h"

#include <QUrl>

namespace dsqt {

DsQmlEnvironment::DsQmlEnvironment(QObject* parent)
    : QObject{parent} {

}

const QString dsqt::DsQmlEnvironment::expand(const QString& string) {
    const std::string val = DsEnvironment::expand(string.toStdString());
    return QString::fromStdString(val);
}

const QUrl DsQmlEnvironment::expandUrl(const QString& string) {
    const QString trimmed = string.trimmed();

    // Network / data URLs must NOT be path-expanded: expand() runs QDir::cleanPath, which
    // collapses the "//" in "http://host/..." and drops the host. Pass these straight through.
    const QString scheme = QUrl(trimmed).scheme();
    if (scheme == QLatin1String("http") || scheme == QLatin1String("https") ||
        scheme == QLatin1String("data")) {
        return QUrl(trimmed);
    }

    // Everything else (local paths, %VAR%-prefixed paths, file:/qrc: URLs) gets variable
    // expansion. A bare local path (empty scheme) or a Windows drive path (single-letter
    // "scheme", e.g. the "c" of "C:/...") must then be turned into a proper file URL.
    const QString expanded = expand(trimmed);
    const QUrl    url(expanded);
    if (url.scheme().isEmpty() || url.scheme().size() == 1) {
        return QUrl::fromLocalFile(expanded);
    }
    return url;
}

const QString DsQmlEnvironment::logFile()
{
    DsGuiApplication* app = dynamic_cast<DsGuiApplication*>(QCoreApplication::instance());
    return app->getLogPath();
}



} // namespace dsqt

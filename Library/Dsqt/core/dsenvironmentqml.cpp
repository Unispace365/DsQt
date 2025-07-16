#include "dsenvironmentqml.h"
namespace dsqt {
DSEnvironmentQML::DSEnvironmentQML(QObject *parent)
    : QObject{parent}
{}

const QString dsqt::DSEnvironmentQML::expand(const QString& string)
{
    const std::string val = DSEnvironment::expand(string.toStdString());
    return QString::fromStdString(val);
}

const QUrl DSEnvironmentQML::expandUrl(const QString &string)
{
    return QUrl(expand(string));
}
}

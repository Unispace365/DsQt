#include "dsReloadUrlInterceptor.h"
#include <QDebug>
#include <QGuiApplication>
#include <QRandomGenerator>

Q_LOGGING_CATEGORY(lgReloadUrl, "reloadUrl")
Q_LOGGING_CATEGORY(lgReloadUrlVerbose, "reloadUrl.verbose")
namespace dsqt {
DsReloadUrlInterceptor::DsReloadUrlInterceptor() {}

void DsReloadUrlInterceptor::setPrefixes(QString proj_from, QString proj_to, QString framework_from, QString framework_to)
{
    mProjectFromPrefix = proj_from;
    mProjectToPrefix = proj_to;
    mFrameworkFromPrefix = framework_from;
    mFrameworkToPrefix = framework_to;
}

QUrl DsReloadUrlInterceptor::intercept(const QUrl &path, DataType type)
{
#ifdef _DEBUG //only for debug.
    if(type == QQmlAbstractUrlInterceptor::DataType::QmlFile){
        qCDebug(lgReloadUrl)<<"Got QML URL "<<path.toString();
        auto pathString = path.toString();
        if(pathString.startsWith(mProjectFromPrefix)){
            pathString.replace(mProjectFromPrefix,mProjectToPrefix);
            if(!pathString.contains("?tip")){
                pathString+="?tip="+QString::number(QRandomGenerator::global()->generate());
            }
            qCDebug(lgReloadUrl)<<"and returning altered QML URL "<<pathString;
            return QUrl(pathString);
        } else if(pathString.startsWith(mFrameworkFromPrefix)){
            pathString.replace(mFrameworkFromPrefix,mFrameworkToPrefix);
            if(!pathString.contains("?tip")){
                pathString+="?tip="+QString::number(QRandomGenerator::global()->generate());
            }
            qCDebug(lgReloadUrl)<<"and returning altered QML URL "<<pathString;
            return QUrl(pathString);
        }
    }
#endif
    return path;
}

}//namespace dsqt

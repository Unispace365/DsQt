#include "ds_qml_obj.h"

namespace dsqt {

DsQmlObj::DsQmlObj(int force,QObject *parent)
    : QObject{parent}
{}

DsQmlObj *DsQmlObj::create(QQmlEngine *qmlEngine, QJSEngine *)
{
    auto obj = new DsQmlObj(0);
    obj->mEngine = dynamic_cast<DSQmlApplicationEngine*>(qmlEngine);
    if(obj->mEngine == nullptr)
    {
        qWarning() << "Engine is not a DSQmlApplicationEngine or a subclass. $DS functionality will not be available";
    }
    return obj;
}

DSSettingsProxy* DsQmlObj::appSettings() const
{
    if (!mEngine) return nullptr;
    return mEngine->getAppSettingsProxy();
}

DSEnvironmentQML* DsQmlObj::env() const
{
    if (!mEngine) return nullptr;
    return mEngine->getEnvQml();
}

DSQmlApplicationEngine *DsQmlObj::engine() const
{
    if (!mEngine) return nullptr;
    return mEngine;
}

}

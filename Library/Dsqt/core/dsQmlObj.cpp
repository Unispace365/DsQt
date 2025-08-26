#include "core/dsQmlObj.h"

Q_LOGGING_CATEGORY(lgQmlObj, "core.qmlobj");
Q_LOGGING_CATEGORY(lgQmlObjVerbose, "core.qmlobj.verbose");
namespace dsqt {

namespace {
    const QString DATE_TIME_FORMAT = "yyyy-MM-ddTHH:mm:ss";
}

DsQmlObj::DsQmlObj(QQmlEngine* qmlEngine, QJSEngine* jsEngine, QObject* parent)
    : QObject{parent} {
    mPath   = new DsQmlPathHelper(this);
    mEngine = dynamic_cast<DsQmlApplicationEngine*>(qmlEngine);
    if (mEngine == nullptr) {
        qWarning() << "Engine is not a DsQmlApplicationEngine or a subclass. $DS functionality will not be available";
    } else {
        connect(mEngine, &DsQmlApplicationEngine::rootUpdated, this, &DsQmlObj::updatePlatform);
    }
    updatePlatform();
}

DsQmlObj* DsQmlObj::create(QQmlEngine* qmlEngine, QJSEngine* jsEngine) {
    return new DsQmlObj(qmlEngine, jsEngine);
}

DsQmlSettingsProxy* DsQmlObj::appSettings() const {
    if (!mEngine) return nullptr;
    return mEngine->getAppSettingsProxy();
}

DsQmlEnvironment* DsQmlObj::env() const {
    if (!mEngine) return nullptr;
    return mEngine->getEnvQml();
}

DsQmlApplicationEngine* DsQmlObj::engine() const {
    if (!mEngine) return nullptr;
    return mEngine;
}

model::DsQmlContentModel* DsQmlObj::platform() {
    return mPlatformQml;
}

DsQmlPathHelper* DsQmlObj::path() const {
    return mPath;
}

void DsQmlObj::updatePlatform() {
    if (!mEngine) return;

    qDebug() << "Updating platform";

    auto platform = mEngine->getContentHelper()->getPlatform();
    if (platform != mPlatform) {
        mPlatform    = platform.duplicate();
        mPlatformQml = mEngine->getReferenceMap()->value(mPlatform.getId());
        emit platformChanged();
    }
}

model::DsQmlContentModel* DsQmlObj::getRecordById(QString id) const {
    if (id.isEmpty()) return nullptr;
    return mEngine->getReferenceMap()->value(id);
}

} // namespace dsqt

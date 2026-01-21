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
    // } else {
    //     connect(mEngine, &DsQmlApplicationEngine::bridgeChanged, this, &DsQmlObj::updatePlatform);
    }
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

// model::ContentModel* DsQmlObj::platform() {
//     return mPlatform;
// }

DsQmlPathHelper* DsQmlObj::path() const {
    return mPath;
}

// model::ContentModel* DsQmlObj::getRecordById(const QString& id) const {
//     auto& lookup = model::ContentLookup::get();
//     return lookup.find(id).value();
// }

// void DsQmlObj::updatePlatform() {
//     if (!mEngine) return;

//     qDebug() << "Updating platform";

//     const auto platformUid = appSettings()->getString("platform.id").toString();
//     const auto platform    = model::ContentModel::find(platformUid);
//     if (platform != mPlatform) {
//         mPlatform = platform;
//         emit platformChanged();
//     }
// }

} // namespace dsqt

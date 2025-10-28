#include "core/dsQmlEnvironment.h"
#include "core/dsQmlApplicationEngine.h"
#include "core/dsEnvironment.h"

namespace dsqt {

DsQmlEnvironment::DsQmlEnvironment(QObject* parent)
    : QObject{parent} {
    auto engine = DsQmlApplicationEngine::DefEngine();

    // Listen to content updates.
    connect(engine, &DsQmlApplicationEngine::bridgeChanged, this, &DsQmlEnvironment::updateNow);
}

const QString dsqt::DsQmlEnvironment::expand(const QString& string) {
    const std::string val = DsEnvironment::expand(string.toStdString());
    return QString::fromStdString(val);
}

const QUrl DsQmlEnvironment::expandUrl(const QString& string) {
    return QUrl(expand(string));
}

void DsQmlEnvironment::updateNow() {
    auto       engine      = DsQmlApplicationEngine::DefEngine();
    const auto platformUid = engine->getAppSettings()->getOr<QString>("platform.id", "");
    const auto platform    = model::ContentModel::find(platformUid);
    if (platform) setPlatformName(platform->getName());
}

} // namespace dsqt

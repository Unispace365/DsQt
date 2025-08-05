#include "core/dsQmlEnvironment.h"
#include "core/DsQmlApplicationEngine.h"
#include "core/dsEnvironment.h"

namespace dsqt {

DsQmlEnvironment::DsQmlEnvironment(QObject* parent)
    : QObject{parent} {
    auto engine = DsQmlApplicationEngine::DefEngine();

    // Listen to content updates.
    connect(engine, &DsQmlApplicationEngine::rootUpdated, this, &DsQmlEnvironment::updateNow,
            Qt::ConnectionType::QueuedConnection);
}

const QString dsqt::DsQmlEnvironment::expand(const QString& string) {
    const std::string val = DsEnvironment::expand(string.toStdString());
    return QString::fromStdString(val);
}

const QUrl DsQmlEnvironment::expandUrl(const QString& string) {
    return QUrl(expand(string));
}

void DsQmlEnvironment::updateNow() {
    auto engine    = DsQmlApplicationEngine::DefEngine();
    auto content   = engine->getContentRoot();
    auto platforms = content.getChildByName("platforms");

    // TODO select proper platform by UID.
    auto platform = platforms.getChild(0);
    setPlatformName(platform.getPropertyString("record_name"));
}

} // namespace dsqt

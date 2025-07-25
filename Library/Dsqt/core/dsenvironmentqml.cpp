#include "dsenvironmentqml.h"

#include "dsqmlapplicationengine.h"

namespace dsqt {

DSEnvironmentQML::DSEnvironmentQML(QObject* parent)
    : QObject{parent} {
    auto engine = DSQmlApplicationEngine::DefEngine();

    connect(
        engine, &DSQmlApplicationEngine::rootUpdated, this,
        [this, engine]() {
            // Platform name.
            auto content   = engine->getContentRoot();
            auto platforms = content.getChildByName("platforms");
            auto platform  = platforms.getChild(0);
            setPlatformName(platform.getPropertyString("record_name"));
        },
        Qt::ConnectionType::QueuedConnection);
}

const QString dsqt::DSEnvironmentQML::expand(const QString& string) {
    const std::string val = DSEnvironment::expand(string.toStdString());
    return QString::fromStdString(val);
}

const QUrl DSEnvironmentQML::expandUrl(const QString& string) {
    return QUrl(expand(string));
}

QString DSEnvironmentQML::getPlatformName() const {
    return m_platformName;
}

void DSEnvironmentQML::setPlatformName(const QString& name) {
    if (!name.isEmpty() && name != m_platformName) {
        m_platformName = name;
        emit platformNameChanged();
    }
}

} // namespace dsqt

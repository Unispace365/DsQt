#include "dsQmlSettingsProxy.h"

#include "settings/dsSettings.h"

namespace dsqt {

DsQmlSettingsProxy::DsQmlSettingsProxy(QObject* parent)
    : QObject(parent)
{}

void DsQmlSettingsProxy::setTarget(const QString& target) {
    if (m_target == target)
        return;
    m_target = target;
    if (!m_target.isEmpty())
        Settings::add(m_target);
    emit targetChanged();
}

void DsQmlSettingsProxy::setPrefix(const QString& prefix) {
    if (m_prefix == prefix)
        return;
    m_prefix = prefix;
    emit prefixChanged();
}

QString DsQmlSettingsProxy::fullKey(const QString& key) const {
    if (m_prefix.isEmpty())
        return key;
    return m_prefix + u'.' + key;
}

template<typename T>
QVariant DsQmlSettingsProxy::getAs(const QString& key, const QVariant& def) const {
    if (m_target.isEmpty())
        return def;
    const QVariant raw = Settings::find<QVariant>(m_target, fullKey(key));
    if (!raw.isValid() || !raw.canConvert<T>())
        return def;
    return QVariant::fromValue(raw.value<T>());
}

QVariant DsQmlSettingsProxy::getString  (const QString& key, const QVariant& def) const { return getAs<QString>  (key, def); }
QVariant DsQmlSettingsProxy::getInt     (const QString& key, const QVariant& def) const { return getAs<int>      (key, def); }
QVariant DsQmlSettingsProxy::getFloat   (const QString& key, const QVariant& def) const { return getAs<double>   (key, def); }
QVariant DsQmlSettingsProxy::getBool    (const QString& key, const QVariant& def) const { return getAs<bool>     (key, def); }
QVariant DsQmlSettingsProxy::getDate    (const QString& key, const QVariant& def) const { return getAs<QDate>    (key, def); }
QVariant DsQmlSettingsProxy::getTime    (const QString& key, const QVariant& def) const { return getAs<QTime>    (key, def); }
QVariant DsQmlSettingsProxy::getDateTime(const QString& key, const QVariant& def) const { return getAs<QDateTime>(key, def); }
QVariant DsQmlSettingsProxy::getColor   (const QString& key, const QVariant& def) const { return getAs<QColor>   (key, def); }
QVariant DsQmlSettingsProxy::getPoint   (const QString& key, const QVariant& def) const { return getAs<QPointF>  (key, def); }
QVariant DsQmlSettingsProxy::getSize    (const QString& key, const QVariant& def) const { return getAs<QSizeF>   (key, def); }
QVariant DsQmlSettingsProxy::getRect    (const QString& key, const QVariant& def) const { return getAs<QRectF>   (key, def); }
QVariant DsQmlSettingsProxy::getVec3    (const QString& key, const QVariant& def) const { return getAs<QVector3D>(key, def); }
QVariant DsQmlSettingsProxy::getVec4    (const QString& key, const QVariant& def) const { return getAs<QVector4D>(key, def); }
QVariant DsQmlSettingsProxy::getQuat    (const QString& key, const QVariant& def) const { return getAs<QQuaternion>(key, def); }

QVariantList DsQmlSettingsProxy::getList(const QString& key, const QVariant& def) const {
    return Settings::find<QVariantList>(m_target, fullKey(key), def.toList());
}

QVariantMap DsQmlSettingsProxy::getObj(const QString& key, const QVariant& def) const {
    return Settings::find<QVariantMap>(m_target, fullKey(key), def.toMap());
}

} // namespace dsqt

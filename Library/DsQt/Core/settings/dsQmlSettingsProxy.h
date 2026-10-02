#pragma once

#include <QColor>
#include <QDateTime>
#include <QObject>
#include <QPointF>
#include <QQmlEngine>
#include <QQuaternion>
#include <QRectF>
#include <QSizeF>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QVector3D>
#include <QVector4D>

namespace dsqt {

class DsQmlSettingsProxy : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(DsSettingsProxy)

    Q_PROPERTY(QString target READ target WRITE setTarget NOTIFY targetChanged)
    Q_PROPERTY(QString prefix READ prefix WRITE setPrefix NOTIFY prefixChanged)

public:
    explicit DsQmlSettingsProxy(QObject* parent = nullptr);

    QString target() const { return m_target; }
    // Sets the name of the settings file to use. Calls Settings::add(target)
    // to ensure the file is registered and loaded before the first lookup.
    void setTarget(const QString& target);

    QString prefix() const { return m_prefix; }
    // Sets the key prefix. Every key passed to get*() is looked up as
    // "<prefix>.<key>" (or just "<key>" when prefix is empty).
    void setPrefix(const QString& prefix);

    Q_INVOKABLE QVariant getString  (const QString& key, const QVariant& def = {}) const;
    Q_INVOKABLE QVariant getInt     (const QString& key, const QVariant& def = {}) const;
    Q_INVOKABLE QVariant getFloat   (const QString& key, const QVariant& def = {}) const;
    Q_INVOKABLE QVariant getBool    (const QString& key, const QVariant& def = {}) const;
    Q_INVOKABLE QVariant getDate    (const QString& key, const QVariant& def = {}) const;
    Q_INVOKABLE QVariant getTime    (const QString& key, const QVariant& def = {}) const;
    Q_INVOKABLE QVariant getDateTime(const QString& key, const QVariant& def = {}) const;
    Q_INVOKABLE QVariant getColor   (const QString& key, const QVariant& def = {}) const;
    Q_INVOKABLE QVariant getPoint   (const QString& key, const QVariant& def = {}) const;
    Q_INVOKABLE QVariant getSize    (const QString& key, const QVariant& def = {}) const;
    Q_INVOKABLE QVariant getRect    (const QString& key, const QVariant& def = {}) const;
    Q_INVOKABLE QVariant getVec3    (const QString& key, const QVariant& def = {}) const;
    Q_INVOKABLE QVariant getVec4    (const QString& key, const QVariant& def = {}) const;
    Q_INVOKABLE QVariant getQuat    (const QString& key, const QVariant& def = {}) const;
    Q_INVOKABLE QVariantList getList(const QString& key, const QVariant& def = {}) const;
    Q_INVOKABLE QVariantMap  getObj (const QString& key, const QVariant& def = {}) const;

signals:
    void targetChanged();
    void prefixChanged();

private:
    // Returns prefix + "." + key, or just key when prefix is empty.
    QString fullKey(const QString& key) const;

    // Fetches the raw value via Settings::find<QVariant>, then converts to T.
    // Returns def unchanged if the target is unset, the key is missing, or
    // the value cannot be converted to T.
    template<typename T>
    QVariant getAs(const QString& key, const QVariant& def) const;

    QString m_target;
    QString m_prefix;
};

} // namespace dsqt

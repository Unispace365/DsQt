#ifndef DSSETTINGPROXY_H
#define DSSETTINGPROXY_H

#include <QObject>
#include <qqmlintegration.h>
#include "settings/dssettings.h"

namespace dsqt {
///
/// \brief The DSSettingsProxy class
///
class DSSettingsProxy : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QString target READ target WRITE setTarget NOTIFY targetChanged);
	Q_PROPERTY(QString prefix READ prefix WRITE setPrefix NOTIFY prefixChanged);
	QML_ELEMENT
  public:
	explicit DSSettingsProxy(QObject *parent = nullptr);

	QString target(){return _target;}
	void setTarget(const QString& val);

	QString prefix(){return _prefix;}
	void setPrefix(const QString& val);

	Q_INVOKABLE void loadFromFile(const QString& filename);
	Q_INVOKABLE QVariant getString(const QString& key,QVariant def = QVariant{});
	Q_INVOKABLE QVariant getInt(const QString& key,QVariant def = QVariant{});
	Q_INVOKABLE QVariant getFloat(const QString& key,QVariant def = QVariant{});
	Q_INVOKABLE QVariant getDate(const QString& key,QVariant def = QVariant{});
	Q_INVOKABLE QVariant getPoint(const QString& key,QVariant def = QVariant{});
	Q_INVOKABLE QVariant getRect(const QString& key,QVariant def = QVariant{});
	Q_INVOKABLE QVariant getSize(const QString& key,QVariant def = QVariant{});
	Q_INVOKABLE QVariant getColor(const QString& key,QVariant def = QVariant{});
	Q_INVOKABLE QVariant getBool(const QString& key, QVariant def = QVariant{});

    Q_INVOKABLE QVariantList getList(const QString &key, QVariant def = QVariant{});
    Q_INVOKABLE QVariantMap getObj(const QString &key, QVariant def = QVariant{});
signals:
	void targetChanged(QString target);
	void prefixChanged(QString prefix);

  private:
	QString _target;
	QString _prefix="";
	std::string _prefix_str="";
	DSSettingsRef _settings;

};
}
#endif // DSSETTINGPROXY_H

#ifndef DSCONTENTMODEL_H
#define DSCONTENTMODEL_H

#include <QLoggingCategory>
#include <QQmlPropertyMap>
#include <qqml.h>
#include <QtSql/QSql>
#include <QHash>
#include "dscontentmodelmodel.h"

Q_DECLARE_LOGGING_CATEGORY(contentModel)
Q_DECLARE_LOGGING_CATEGORY(contentModelWarn)
namespace dsqt::model {
class DSContentModel;

typedef std::shared_ptr<DSContentModel> DSContentModelPtr;
class DSContentModel: public QQmlPropertyMap
{
    Q_OBJECT
	Q_PROPERTY(QQmlListProperty<DSContentModel> children READ getChildren NOTIFY childrenChanged)

    QML_ELEMENT
public:
	explicit DSContentModel(QObject *parent=nullptr, QString name="");
	Q_INVOKABLE QQmlListProperty<dsqt::model::DSContentModel> getChildrenByName(QString name);
	Q_INVOKABLE dsqt::model::DSContentModel* getChildByName(QString name);
	Q_INVOKABLE dsqt::model::DSContentModelModel* getModel();
	QQmlListProperty<DSContentModel> getChildren();
	static DSContentModelPtr mContent;

	void lock() {
		mImmutable = true;
		auto children = findChildren<DSContentModel*>();
		for(DSContentModel* child:children){
			child->mImmutable = true;
		}
	}
	void unlock(){
		mImmutable = false;
		auto children = findChildren<DSContentModel*>();
		for(DSContentModel* child:children){
			child->mImmutable = false;
		}
	}

  signals:
	void childrenChanged();

  protected:


    // QQmlPropertyMap interface
protected:

	QVariant updateValue(const QString &key, const QVariant &input) override;

private:
	static void childrenAppend(QQmlListProperty<DSContentModel>*,DSContentModel*);
	static qsizetype childrenCount(QQmlListProperty<DSContentModel>*);
	static DSContentModel* child(QQmlListProperty<DSContentModel>*, qsizetype);
	static void childrenClear(QQmlListProperty<DSContentModel>*);
	static void childrenReplace(QQmlListProperty<DSContentModel>*,qsizetype,DSContentModel*);
	static void childrenRemoveLast(QQmlListProperty<DSContentModel>*);
	QHash<QString,QString> mSearchName;
	bool mImmutable = true;
	QQmlListProperty<DSContentModel> m_children;
	DSContentModelModel* mModel = nullptr;
};

}//namespace ds::model

#endif // DSCONTENTMODEL_H

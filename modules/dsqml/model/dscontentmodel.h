#ifndef DSCONTENTMODEL_H
#define DSCONTENTMODEL_H

#include <qqml.h>
#include <QHash>
#include <QLoggingCategory>
#include <QQmlPropertyMap>
#include <QtSql/QSql>
#include "dscontentItemmodel.h"

Q_DECLARE_LOGGING_CATEGORY(contentModel)
Q_DECLARE_LOGGING_CATEGORY(contentModelWarn)
namespace dsqt::model {
class DSContentModel;

typedef std::shared_ptr<DSContentModel> DSContentModelPtr;

class DSContentModel: public QQmlPropertyMap
{
    Q_OBJECT
	Q_PROPERTY(QQmlListProperty<DSContentModel> children READ getChildren NOTIFY childrenChanged)
	Q_PROPERTY(QString id READ id NOTIFY idChanged FINAL)
	QML_ELEMENT
public:
  Q_INVOKABLE QQmlListProperty<dsqt::model::DSContentModel> getChildrenByName(QString name);
  Q_INVOKABLE dsqt::model::DSContentModel* getChildByName(QString name);
  Q_INVOKABLE dsqt::model::DSContentItemModel*	getModel();
  QQmlListProperty<DSContentModel>				getChildren();
  void											addChild(DSContentModelPtr child);
  static DSContentModelPtr						create(QString name = "", QObject* parent = mContent.get());
  static DSContentModelPtr						getRoot();

  void lock() {
	  mImmutable	= true;
	  auto children = findChildren<DSContentModel*>();
	  for (DSContentModel* child : children) {
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

	QString id() const;
	void	setId(const QString& newId);

  signals:
	void childrenChanged();
	void idChanged();

  protected:
	// QQmlPropertyMap interface
  protected:

	QVariant updateValue(const QString &key, const QVariant &input) override;

private:
  static DSContentModelPtr mContent;
  explicit DSContentModel(QObject* parent = nullptr, QString name = "");
  static void					   childrenAppend(QQmlListProperty<DSContentModel>*, DSContentModel*);
  static qsizetype				   childrenCount(QQmlListProperty<DSContentModel>*);
  static DSContentModel*		   child(QQmlListProperty<DSContentModel>*, qsizetype);
  static void					   childrenClear(QQmlListProperty<DSContentModel>*);
  static void					   childrenReplace(QQmlListProperty<DSContentModel>*, qsizetype, DSContentModel*);
  static void					   childrenRemoveLast(QQmlListProperty<DSContentModel>*);
  QHash<QString, QString>		   mSearchName;
  bool							   mImmutable = true;
  QQmlListProperty<DSContentModel> m_children;
  DSContentItemModel*			   mModel = nullptr;
  QString						   m_id;
};

}//namespace ds::model

#endif // DSCONTENTMODEL_H

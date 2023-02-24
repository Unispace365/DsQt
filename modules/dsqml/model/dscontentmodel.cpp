#include "dscontentmodel.h"
#include "dscontentmodelmodel.h"
#include <QQmlEngine>

Q_LOGGING_CATEGORY(contentModel, "content.model")
Q_LOGGING_CATEGORY(contentModelWarn, "content.model.warning")
namespace dsqt::model {
DSContentModelPtr DSContentModel::mContent = DSContentModelPtr(new DSContentModel(nullptr,"root"));

DSContentModel::DSContentModel(QObject* parent,QString name):QQmlPropertyMap(this,parent){
	setObjectName(name);
}

QQmlListProperty<DSContentModel> DSContentModel::getChildrenByName(QString name)
{

	mSearchName[name] = name;
	return QQmlListProperty<DSContentModel>(this, &mSearchName[name],
											&DSContentModel::childrenAppend,
											&DSContentModel::childrenCount,
											&DSContentModel::child,
											&DSContentModel::childrenClear,
											&DSContentModel::childrenReplace,
											&DSContentModel::childrenRemoveLast
											);
}

DSContentModel *DSContentModel::getChildByName(QString name)
{
	return findChild<DSContentModel*>(name,Qt::FindDirectChildrenOnly);
}

DSContentModelModel *DSContentModel::getModel()
{
	if(!mModel){
		mModel = new DSContentModelModel(this,this);
	}
	return mModel;
}

QVariant DSContentModel::updateValue(const QString &key, const QVariant &input)
{
	if(mImmutable){
		qWarning()<<"Attempting to set value for '"<<key<<"' but DSContentModel is immutable";
		return this->value(key);
	} else {
		return QQmlPropertyMap::updateValue(key,input);
	}
}



QQmlListProperty<DSContentModel> DSContentModel::getChildren()
{
	return QQmlListProperty<DSContentModel>(this, nullptr,
			&DSContentModel::childrenAppend,
		&DSContentModel::childrenCount,
		&DSContentModel::child,
		&DSContentModel::childrenClear,
		&DSContentModel::childrenReplace,
		&DSContentModel::childrenRemoveLast
											);
}

void DSContentModel::childrenAppend(QQmlListProperty<DSContentModel> *l, DSContentModel *m)
{
	qmlEngine(l->object)->throwError(tr("You can append children to content model in QML"));
}

qsizetype DSContentModel::childrenCount(QQmlListProperty<DSContentModel> *l)
{
	QString* name = (QString*)l->data;
	if(name){
		return l->object->findChildren<DSContentModel*>(*name,Qt::FindDirectChildrenOnly).length();
	}
	return l->object->findChildren<DSContentModel*>(Qt::FindDirectChildrenOnly).length();
}

DSContentModel *DSContentModel::child(QQmlListProperty<DSContentModel> *l, qsizetype index)
{
	QString* name = (QString*)l->data;
	if(name){
		return l->object->findChildren<DSContentModel*>(*name,Qt::FindDirectChildrenOnly).at(index);
	}
	return l->object->findChildren<DSContentModel*>(Qt::FindDirectChildrenOnly).at(index);
}

void DSContentModel::childrenClear(QQmlListProperty<DSContentModel> *l)
{

	qmlEngine(l->object)->throwError(tr("You can clear children of a content model in QML"));
}

void DSContentModel::childrenReplace(QQmlListProperty<DSContentModel> *l, qsizetype, DSContentModel *)
{
	qmlEngine(l->object)->throwError(tr("You can replace children of a content model in QML"));
}

void DSContentModel::childrenRemoveLast(QQmlListProperty<DSContentModel> *l)
{
	qmlEngine(l->object)->throwError(tr("You can remove children of a content model in QML"));
}




}

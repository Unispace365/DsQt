#include "dscontentmodelmodel.h"
#include "dscontentmodel.h"
namespace dsqt::model {
DSContentItemModel::DSContentItemModel(DSContentModel* data, QObject *parent)
  : QAbstractItemModel{parent}
{
	mRoot = data;
}



DSContentItemModel::~DSContentItemModel()
{
   //
}

QVariant DSContentItemModel::data(const QModelIndex &index, int role) const
{
	if(!index.isValid()){
		return QVariant();
	}

	if(role != Qt::DisplayRole) {
		return QVariant();
	}

	DSContentModel* content = static_cast<DSContentModel*>(index.internalPointer());

	return QVariant::fromValue(content);
}

Qt::ItemFlags DSContentItemModel::flags(const QModelIndex &index) const
{
	if (!index.isValid())
		return Qt::NoItemFlags;

	return QAbstractItemModel::flags(index);
}

QVariant DSContentItemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
		return QVariant(QString("name"));

	return QVariant();
}

QModelIndex DSContentItemModel::index(int row, int column, const QModelIndex &parent) const
{
	if (!hasIndex(row, column, parent))
		return QModelIndex();

	DSContentModel *parentItem;

	if (!parent.isValid())
		parentItem = mRoot;
	else
		parentItem = static_cast<DSContentModel*>(parent.internalPointer());

	DSContentModel *childItem = parentItem->findChildren<DSContentModel*>(Qt::FindDirectChildrenOnly).at(row);
	if (childItem)
		return createIndex(row, column, childItem);
	return QModelIndex();
}

QModelIndex DSContentItemModel::parent(const QModelIndex &index) const
{
	if (!index.isValid())
		return QModelIndex();

	DSContentModel *childItem = static_cast<DSContentModel*>(index.internalPointer());
	DSContentModel *parentItem = static_cast<DSContentModel*>(childItem->parent());

	if (parentItem == mRoot)
		return QModelIndex();

	int row = 0;
	auto grandParentItem = parentItem->parent();
	if(grandParentItem){
		row = grandParentItem->findChildren<DSContentModel*>(Qt::FindDirectChildrenOnly).indexOf(parentItem);
	}

	return createIndex(row, 0, parentItem);
}

int DSContentItemModel::rowCount(const QModelIndex &parent) const
{
	DSContentModel *parentItem;
	if (parent.column() > 0)
		return 0;

	if (!parent.isValid())
		parentItem = mRoot;
	else
		parentItem = static_cast<DSContentModel*>(parent.internalPointer());

	return parentItem->findChildren<DSContentModel*>(Qt::FindDirectChildrenOnly).length();
}

int DSContentItemModel::columnCount(const QModelIndex &parent) const
{
	return 1;
}


}


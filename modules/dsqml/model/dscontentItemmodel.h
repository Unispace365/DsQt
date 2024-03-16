#ifndef DSCONTENTITEMMODEL_H
#define DSCONTENTITEMMODEL_H

#include <QObject>
#include <QAbstractItemModel>
#include <QModelIndex>
#include <qqml.h>
namespace dsqt::model {
class DSContentModel;
class DSContentItemModel : public QAbstractItemModel {
	Q_OBJECT
	QML_ELEMENT

  public:
	explicit DSContentModelModel(DSContentModel* data=nullptr, QObject *parent = nullptr);
	~DSContentModelModel();

	// QAbstractItemModel interface
  public:
	QVariant data(const QModelIndex &index, int role) const override;
	Qt::ItemFlags flags(const QModelIndex &index) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role=Qt::DisplayRole) const override;
	QModelIndex index(int row, int column, const QModelIndex &parent) const;
	QModelIndex parent(const QModelIndex &child) const;
	int rowCount(const QModelIndex &parent) const;
	int columnCount(const QModelIndex &parent) const;

  private:
	DSContentModel *mRoot;
};
}

#endif // DSCONTENTITEMMODEL_H

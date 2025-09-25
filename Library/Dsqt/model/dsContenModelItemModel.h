#ifndef DSCONTENMODELITEMMODEL_H
#define DSCONTENMODELITEMMODEL_H

#include "core/dsQmlApplicationEngine.h"
#include "rework/rwContentModel.h"
#include <QAbstractItemModel>

namespace dsqt::model {

struct ContentModelItem {
    ContentModelItem* parentItem();
    ContentModelItem* child(int row);
    int               childCount() const;
    int               columnCount() const;
    QVariant          data(int role) const;
    int               row() const;

    const ContentModel*                            m_model = nullptr;
    std::vector<std::unique_ptr<ContentModelItem>> m_childItems;
    ContentModelItem*                              m_parentItem = nullptr;
};

class DsContenModelItemModel : public QAbstractItemModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(DsContentModelItemModel);
    Q_PROPERTY(bool isDirty READ isDirty WRITE setIsDirty NOTIFY isDirtyChanged FINAL)
  public:
    explicit DsContenModelItemModel(QObject* parent = nullptr);
    ~DsContenModelItemModel() override = default;

    // Implement required methods for QAbstractItemModel
    QModelIndex            index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex            parent(const QModelIndex& index) const override;
    int                    rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int                    columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant               data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags          flags(const QModelIndex& index) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE void       reload();

    bool isDirty() const;
    void setIsDirty(bool newIsDirty);

  signals:
    void isDirtyChanged();

  private:
    dsqt::DsQmlApplicationEngine*     mEngine = nullptr;
    std::unique_ptr<ContentModelItem> updateModelData(const ContentModel* model);
    void                              updateModelData();

    std::unique_ptr<ContentModelItem> m_rootItem;
    bool                              m_isDirty;
};
} // namespace dsqt::model
#endif // DSCONTENMODELITEMMODEL_H

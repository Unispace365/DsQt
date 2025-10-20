#ifndef DSCONTENTMODELITEMMODEL_H
#define DSCONTENTMODELITEMMODEL_H

#include "core/dsQmlApplicationEngine.h"
#include "model/dsContentModel.h"
#include <QAbstractItemModel>

namespace dsqt::model {

class ContentModelItem {
  public:
    ContentModelItem() = default;
    ContentModelItem(const ContentModel* ptr, ContentModelItem* parent = nullptr);

    const ContentModel* model() const { return m_model.get(); }
    const auto&         children() const { return m_childItems; }

    ContentModelItem* parentItem();
    ContentModelItem* child(int row);
    int               childCount() const;
    int               columnCount() const;
    QVariant          data(int role) const;
    int               row() const;

    bool reset(const ContentModel* ptr);
    void replace(const std::vector<std::shared_ptr<ContentModelItem>>& children) { m_childItems = children; }

  private:
    QPointer<const ContentModel>                   m_model;
    ContentModelItem*                              m_parentItem = nullptr;
    std::vector<std::shared_ptr<ContentModelItem>> m_childItems;
};

class DsContentModelItemModel : public QAbstractItemModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(DsContentModelItemModel);
    Q_PROPERTY(bool isDirty READ isDirty WRITE setIsDirty NOTIFY isDirtyChanged FINAL)

  public:
    explicit DsContentModelItemModel(QObject* parent = nullptr);
    ~DsContentModelItemModel() override = default;

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
    void                              update();
    std::shared_ptr<ContentModelItem> createModelItem(const ContentModel* model,
                                                      ContentModelItem*   parentItem = nullptr);
    void updateModelData(const ContentModel* model,  std::shared_ptr<ContentModelItem> parentItem, const QModelIndex& parentIndex);

    dsqt::DsQmlApplicationEngine*     mEngine = nullptr;
    std::shared_ptr<ContentModelItem> m_rootItem;
    bool                              m_isDirty;
};
} // namespace dsqt::model
#endif // DSCONTENTMODELITEMMODEL_H

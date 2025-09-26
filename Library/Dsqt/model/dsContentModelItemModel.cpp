#include "model/dsContentModelItemModel.h"

namespace dsqt::model {

DsContentModelItemModel::DsContentModelItemModel(QObject* parent)
    : QAbstractItemModel{parent} {
    mEngine = DsQmlApplicationEngine::DefEngine();
    connect(mEngine, &DsQmlApplicationEngine::bridgeChanged, this, [this]() {
        if (m_rootItem) {
            bool doReload = m_rootItem->childCount() == 0;
            doReload      = doReload || m_rootItem->childCount() > 0 ? m_rootItem->child(0)->childCount() > 0 : false;
            if (doReload) {
                reload();
                return;
            }
        }
        setIsDirty(true);
    });
    updateModelData();
}

QModelIndex DsContentModelItemModel::index(int row, int column, const QModelIndex& parent) const {
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }
    ContentModelItem* parentItem =
        parent.isValid() ? static_cast<ContentModelItem*>(parent.internalPointer()) : m_rootItem.get();

    if (auto* childItem = parentItem->child(row)) {
        return createIndex(row, column, childItem);
    }
    return {};
}

QModelIndex DsContentModelItemModel::parent(const QModelIndex& index) const {
    if (!index.isValid()) return {};

    auto*             childItem  = static_cast<ContentModelItem*>(index.internalPointer());
    ContentModelItem* parentItem = childItem->parentItem();

    return parentItem != m_rootItem.get() ? createIndex(parentItem->row(), 0, parentItem) : QModelIndex{};
}

int DsContentModelItemModel::rowCount(const QModelIndex& parent) const {
    if (parent.column() > 0) return 0;

    const ContentModelItem* parentItem =
        parent.isValid() ? static_cast<const ContentModelItem*>(parent.internalPointer()) : m_rootItem.get();

    return parentItem->childCount();
}

int DsContentModelItemModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return static_cast<ContentModelItem*>(parent.internalPointer())->columnCount();
    return m_rootItem->columnCount();
}

QVariant DsContentModelItemModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) {
        return {};
    }

    switch (role) {
    case Qt::DisplayRole:
    case Qt::UserRole:
    case Qt::UserRole + 1:
        break;
    default:
        return {};
    }

    const auto* item = static_cast<const ContentModelItem*>(index.internalPointer());
    return item->data(role);
}

Qt::ItemFlags DsContentModelItemModel::flags(const QModelIndex& index) const {
    return index.isValid() ? QAbstractItemModel::flags(index) : Qt::ItemFlags(Qt::NoItemFlags);
}

QHash<int, QByteArray> DsContentModelItemModel::roleNames() const {
    return QHash<int, QByteArray>{
        {Qt::DisplayRole,  "display"     },
        {Qt::UserRole,     "id"          },
        {Qt::UserRole + 1, "contentModel"}
    };
}

void DsContentModelItemModel::reload() {
    beginResetModel();
    updateModelData();
    endResetModel();
    setIsDirty(false);
}

std::unique_ptr<ContentModelItem> DsContentModelItemModel::updateModelData(const ContentModel* model) {
    const auto name = model ? model->getName() : "[undefined]";

    auto item = new ContentModelItem();
    if (model) {
        item->m_model = model;

        const auto children = model->getChildren();
        for (auto child : children) {
            auto node = updateModelData(child);

            node->m_parentItem = item;
            item->m_childItems.push_back(std::move(node));
        }
    }
    return std::unique_ptr<ContentModelItem>(item);
}

void DsContentModelItemModel::updateModelData() {
    m_rootItem = updateModelData(mEngine->bridge());
}

ContentModelItem* ContentModelItem::parentItem() {
    return m_parentItem;
}

ContentModelItem* ContentModelItem::child(int row) {
    return row >= 0 && row < m_childItems.size() ? m_childItems[row].get() : nullptr;
}

int ContentModelItem::childCount() const {
    return int(m_childItems.size());
}

int ContentModelItem::columnCount() const {
    return 1;
}

QVariant ContentModelItem::data(int role) const {
    switch (role) {
    case Qt::DisplayRole:
        return m_model->getName();
    case Qt::UserRole:
        return m_model->getId();
    case Qt::UserRole + 1:
        return QVariant::fromValue(m_model);
    default:
        qDebug() << "Unhandled role:" << role;
        return {};
    }
}

int ContentModelItem::row() const {
    if (m_parentItem == nullptr) return 0;
    const auto it =
        std::find_if(m_parentItem->m_childItems.cbegin(), m_parentItem->m_childItems.cend(),
                     [this](const std::unique_ptr<ContentModelItem>& treeItem) { return treeItem.get() == this; });

    if (it != m_parentItem->m_childItems.cend()) return std::distance(m_parentItem->m_childItems.cbegin(), it);
    Q_ASSERT(false); // should not happen
    return -1;
}

bool DsContentModelItemModel::isDirty() const {
    return m_isDirty;
}

void DsContentModelItemModel::setIsDirty(bool newIsDirty) {
    if (m_isDirty == newIsDirty) return;
    m_isDirty = newIsDirty;
    emit isDirtyChanged();
}

} // namespace dsqt::model

#include "model/dsContentModelItemModel.h"
#include "bridge/dsBridge.h"

namespace dsqt::model {

DsContentModelItemModel::DsContentModelItemModel(QObject* parent)
    : QAbstractItemModel{parent} {
    auto& bridge = bridge::DsBridge::instance();
    connect(&bridge, &bridge::DsBridge::contentChanged, this, &DsContentModelItemModel::update);
    update();
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
}

std::shared_ptr<ContentModelItem> DsContentModelItemModel::createModelItem(const ContentModel* model,
                                                                           ContentModelItem*   parentItem) {
    return std::make_shared<ContentModelItem>(model, parentItem);
}

void DsContentModelItemModel::updateModelData(const ContentModel* model, std::shared_ptr<ContentModelItem> item,
                                              const QModelIndex& index) {
    if (!item || !model) {
        qWarning() << "Invalid model or item in updateModelData";
        return;
    }

    // Check if item is still valid.
    Q_ASSERT(item->model());
}

void DsContentModelItemModel::update() {
    // if (!m_rootItem) {
    //     beginResetModel();
    //     m_rootItem = createModelItem(mEngine->bridge());
    //     endResetModel();
    // } else {
    //     updateModelData(mEngine->bridge(), m_rootItem, QModelIndex());
    // }
    // setIsDirty(false);
}

ContentModelItem::ContentModelItem(const ContentModel* ptr, ContentModelItem* parent) {
    m_parentItem = parent;
    reset(ptr);
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
        return QVariant::fromValue(m_model.get());
    default:
        qDebug() << "Unhandled role:" << role;
        return {};
    }
}

int ContentModelItem::row() const {
    if (m_parentItem == nullptr) return 0;
    const auto it =
        std::find_if(m_parentItem->m_childItems.cbegin(), m_parentItem->m_childItems.cend(),
                     [this](const std::shared_ptr<ContentModelItem>& treeItem) { return treeItem.get() == this; });

    if (it != m_parentItem->m_childItems.cend()) return std::distance(m_parentItem->m_childItems.cbegin(), it);
    Q_ASSERT(false); // should not happen
    return -1;
}

bool ContentModelItem::reset(const ContentModel* ptr) {
    if (m_model && m_model->isEqualTo(ptr)) return false;

    m_model = ptr;
    m_childItems.clear();

    if (m_model) {
        const auto children = m_model->getChildren();
        for (auto child : children) {
            auto node = std::make_shared<ContentModelItem>(child, this);
            m_childItems.push_back(node);
        }
    }

    return true;
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

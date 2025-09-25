#include "model/dsContenModelItemModel.h"

namespace dsqt::model {

DsContenModelItemModel::DsContenModelItemModel(QObject* parent)
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

QModelIndex DsContenModelItemModel::index(int row, int column, const QModelIndex& parent) const {
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

QModelIndex DsContenModelItemModel::parent(const QModelIndex& index) const {
    if (!index.isValid()) return {};

    auto*             childItem  = static_cast<ContentModelItem*>(index.internalPointer());
    ContentModelItem* parentItem = childItem->parentItem();

    return parentItem != m_rootItem.get() ? createIndex(parentItem->row(), 0, parentItem) : QModelIndex{};
}

int DsContenModelItemModel::rowCount(const QModelIndex& parent) const {
    if (parent.column() > 0) return 0;

    const ContentModelItem* parentItem =
        parent.isValid() ? static_cast<const ContentModelItem*>(parent.internalPointer()) : m_rootItem.get();

    return parentItem->childCount();
}

int DsContenModelItemModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return static_cast<ContentModelItem*>(parent.internalPointer())->columnCount();
    return m_rootItem->columnCount();
}

QVariant DsContenModelItemModel::data(const QModelIndex& index, int role) const {
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

Qt::ItemFlags DsContenModelItemModel::flags(const QModelIndex& index) const {
    return index.isValid() ? QAbstractItemModel::flags(index) : Qt::ItemFlags(Qt::NoItemFlags);
}

QHash<int, QByteArray> DsContenModelItemModel::roleNames() const {
    return QHash<int, QByteArray>{
        {Qt::DisplayRole,  "display"     },
        {Qt::UserRole,     "id"          },
        {Qt::UserRole + 1, "contentModel"}
    };
}

void DsContenModelItemModel::reload() {
    beginResetModel();
    updateModelData();
    endResetModel();
    setIsDirty(false);
}

std::unique_ptr<ContentModelItem> DsContenModelItemModel::updateModelData(const ContentModel* model) {
    auto item = new ContentModelItem();
    if (model) {
        item->m_model = model;
        // item->m_itemData.insert("name", model->getName());
        // item->m_itemData.insert("id", model->getId());

        // const auto var = QVariant::fromValue(model);
        // switch (var.typeId()) {
        // case QMetaType::QStringList:
        //     item->m_itemData.insert("model", var.toStringList().join(","));
        // case QMetaType::QDateTime:
        //     item->m_itemData.insert("model", var.toDateTime().toString("yyyy-MM-dd hh:mm:ss"));
        // case QMetaType::QDate:
        //     item->m_itemData.insert("model", var.toDate().toString("yyyy-MM-dd"));
        // case QMetaType::QTime:
        //     item->m_itemData.insert("model", var.toTime().toString("hh:mm:ss"));
        // default:
        //     item->m_itemData.insert("model", var);
        // }

        const auto children = model->getChildren();
        for (auto child : children) {
            auto node = updateModelData(child);

            node->m_parentItem = item;
            item->m_childItems.push_back(std::move(node));
        }
    }
    return std::unique_ptr<ContentModelItem>(item);
}

void DsContenModelItemModel::updateModelData() {
    auto root = new ContentModelItem();
    m_rootItem.reset(root);

    const auto content = mEngine->bridge()->getProperty<QStringList>("content");
    for (const auto& uid : content) {
        auto node = updateModelData(model::ContentModel::find(uid));

        node->m_parentItem = m_rootItem.get();
        m_rootItem->m_childItems.push_back(std::move(node));
    }

    const auto events = mEngine->bridge()->getProperty<QStringList>("events");
    for (const auto& uid : events) {
        auto node = updateModelData(model::ContentModel::find(uid));

        node->m_parentItem = m_rootItem.get();
        m_rootItem->m_childItems.push_back(std::move(node));
    }

    const auto records = mEngine->bridge()->getProperty<QStringList>("records");
    for (const auto& uid : records) {
        auto node = updateModelData(model::ContentModel::find(uid));

        node->m_parentItem = m_rootItem.get();
        m_rootItem->m_childItems.push_back(std::move(node));
    }
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

bool DsContenModelItemModel::isDirty() const {
    return m_isDirty;
}

void DsContenModelItemModel::setIsDirty(bool newIsDirty) {
    if (m_isDirty == newIsDirty) return;
    m_isDirty = newIsDirty;
    emit isDirtyChanged();
}

} // namespace dsqt::model

#include "model/dsSettingsTreeModel.h"

#include <algorithm>

namespace dsqt {

// --- SettingsTreeItem ---

SettingsTreeItem::SettingsTreeItem(const QString& key, SettingsTreeItem* parent)
    : m_key(key)
    , m_parentItem(parent) {
}

SettingsTreeItem* SettingsTreeItem::child(int row) {
    return row >= 0 && row < static_cast<int>(m_children.size()) ? m_children[row].get() : nullptr;
}

int SettingsTreeItem::childCount() const {
    return static_cast<int>(m_children.size());
}

QVariant SettingsTreeItem::data(int role) const {
    switch (role) {
    case Qt::DisplayRole:    return m_key;
    case Qt::UserRole:       return m_value;
    case Qt::UserRole + 1:   return m_source;
    case Qt::UserRole + 2:   return m_type;
    case Qt::UserRole + 3:   return m_isLeaf;
    default:                 return {};
    }
}

int SettingsTreeItem::row() const {
    if (!m_parentItem) return 0;
    const auto it = std::find_if(
        m_parentItem->m_children.cbegin(), m_parentItem->m_children.cend(),
        [this](const std::shared_ptr<SettingsTreeItem>& item) { return item.get() == this; });
    if (it != m_parentItem->m_children.cend())
        return static_cast<int>(std::distance(m_parentItem->m_children.cbegin(), it));
    Q_ASSERT(false);
    return -1;
}

void SettingsTreeItem::addChild(std::shared_ptr<SettingsTreeItem> child) {
    m_children.push_back(std::move(child));
}

// --- DsSettingsTreeModel ---

DsSettingsTreeModel::DsSettingsTreeModel(QObject* parent)
    : QAbstractItemModel(parent)
    , m_rootItem(std::make_shared<SettingsTreeItem>(QStringLiteral("root"))) {
}

QModelIndex DsSettingsTreeModel::index(int row, int column, const QModelIndex& parent) const {
    if (!hasIndex(row, column, parent)) return {};

    SettingsTreeItem* parentItem =
        parent.isValid() ? static_cast<SettingsTreeItem*>(parent.internalPointer()) : m_rootItem.get();

    if (auto* childItem = parentItem->child(row))
        return createIndex(row, column, childItem);
    return {};
}

QModelIndex DsSettingsTreeModel::parent(const QModelIndex& index) const {
    if (!index.isValid()) return {};

    auto* childItem  = static_cast<SettingsTreeItem*>(index.internalPointer());
    auto* parentItem = childItem->parentItem();

    return parentItem != m_rootItem.get() ? createIndex(parentItem->row(), 0, parentItem) : QModelIndex{};
}

int DsSettingsTreeModel::rowCount(const QModelIndex& parent) const {
    if (parent.column() > 0) return 0;

    const SettingsTreeItem* parentItem =
        parent.isValid() ? static_cast<const SettingsTreeItem*>(parent.internalPointer()) : m_rootItem.get();

    return parentItem->childCount();
}

int DsSettingsTreeModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid())
        return static_cast<SettingsTreeItem*>(parent.internalPointer())->columnCount();
    return m_rootItem->columnCount();
}

QVariant DsSettingsTreeModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) return {};

    switch (role) {
    case Qt::DisplayRole:
    case Qt::UserRole:
    case Qt::UserRole + 1:
    case Qt::UserRole + 2:
    case Qt::UserRole + 3:
        break;
    default:
        return {};
    }

    const auto* item = static_cast<const SettingsTreeItem*>(index.internalPointer());
    return item->data(role);
}

Qt::ItemFlags DsSettingsTreeModel::flags(const QModelIndex& index) const {
    return index.isValid() ? QAbstractItemModel::flags(index) : Qt::ItemFlags(Qt::NoItemFlags);
}

QHash<int, QByteArray> DsSettingsTreeModel::roleNames() const {
    return {
        {Qt::DisplayRole,  "display"},
        {Qt::UserRole,     "value"  },
        {Qt::UserRole + 1, "source" },
        {Qt::UserRole + 2, "type"   },
        {Qt::UserRole + 3, "isLeaf" },
    };
}

void DsSettingsTreeModel::reload() {
    rebuild();
}

void DsSettingsTreeModel::setSettingsName(const QString& name) {
    if (m_settingsName == name) return;
    m_settingsName = name;
    m_settings = DsSettings::getSettings(name.toStdString());
    emit settingsNameChanged();
    rebuild();
}

void DsSettingsTreeModel::setSettings(DsSettings* settings) {
    if (m_settings.get() == settings) return;
    // Find the shared_ptr for this raw pointer from the static map
    for (const auto& name : DsSettings::getSettingsNames()) {
        auto ref = DsSettings::getSettings(name.toStdString());
        if (ref.get() == settings) {
            m_settings = ref;
            m_settingsName = name;
            emit settingsChanged();
            emit settingsNameChanged();
            rebuild();
            return;
        }
    }
    // Not found in static map — shouldn't normally happen, but handle gracefully
    m_settings.reset();
    m_settingsName.clear();
    emit settingsChanged();
    emit settingsNameChanged();
    rebuild();
}

void DsSettingsTreeModel::setFilterFile(const QString& file) {
    if (m_filterFile == file) return;
    m_filterFile = file;
    emit filterFileChanged();
    rebuild();
}

void DsSettingsTreeModel::rebuild() {
    beginResetModel();
    m_rootItem = std::make_shared<SettingsTreeItem>(QStringLiteral("root"));

    if (m_settings) {
        QVariantMap tree = m_settings->getSettingsTree(m_filterFile);
        buildTree(tree, m_rootItem.get(), QString());

        QStringList files = m_settings->getLoadedFiles();
        if (m_loadedFiles != files) {
            m_loadedFiles = files;
            emit loadedFilesChanged();
        }
    }

    endResetModel();
}

void DsSettingsTreeModel::buildTree(const QVariantMap& map, SettingsTreeItem* parent, const QString& parentPath) {
    QStringList keys = map.keys();
    keys.sort(Qt::CaseInsensitive);

    for (const auto& key : keys) {
        const QVariant& val = map[key];
        if (val.typeId() != QMetaType::QVariantMap) continue;

        QVariantMap entry = val.toMap();
        auto item = std::make_shared<SettingsTreeItem>(key, parent);
        item->m_fullKeyPath = parentPath.isEmpty() ? key : parentPath + QStringLiteral(".") + key;

        if (entry.value(QStringLiteral("__isLeaf")).toBool()) {
            item->m_isLeaf = true;
            item->m_value  = entry.value(QStringLiteral("value")).toString();
            item->m_source = entry.value(QStringLiteral("source")).toString();
            item->m_type   = entry.value(QStringLiteral("type")).toString();
        } else {
            buildTree(entry, item.get(), item->m_fullKeyPath);
        }

        parent->addChild(std::move(item));
    }
}

QVariantList DsSettingsTreeModel::search(const QString& text) const {
    QVariantList results;
    if (text.isEmpty()) return results;
    searchRecursive(m_rootItem.get(), text, results);
    return results;
}

void DsSettingsTreeModel::searchRecursive(SettingsTreeItem* item, const QString& text, QVariantList& results) const {
    for (int i = 0; i < item->childCount(); ++i) {
        auto* child = item->child(i);
        bool matches = child->m_key.contains(text, Qt::CaseInsensitive)
                    || child->m_value.contains(text, Qt::CaseInsensitive)
                    || child->m_fullKeyPath.contains(text, Qt::CaseInsensitive);

        if (matches) {
            results.append(QVariantMap{
                {QStringLiteral("key"),   child->m_key},
                {QStringLiteral("path"),  child->m_fullKeyPath},
                {QStringLiteral("value"), child->m_value},
                {QStringLiteral("type"),  child->m_type},
                {QStringLiteral("isLeaf"), child->m_isLeaf},
            });
        }

        if (child->childCount() > 0) {
            searchRecursive(child, text, results);
        }
    }
}

QModelIndex DsSettingsTreeModel::indexForPath(const QString& dottedPath) const {
    QStringList segments = dottedPath.split(QLatin1Char('.'));
    SettingsTreeItem* current = m_rootItem.get();

    for (const auto& segment : segments) {
        bool found = false;
        for (int i = 0; i < current->childCount(); ++i) {
            if (current->child(i)->m_key == segment) {
                current = current->child(i);
                found = true;
                break;
            }
        }
        if (!found) return {};
    }

    if (current == m_rootItem.get()) return {};
    return createIndex(current->row(), 0, current);
}

} // namespace dsqt

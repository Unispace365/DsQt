#include "bridge/dsContentTreeModel.h"
#include "bridge/dsQmlBridge.h"

#include <functional>

namespace dsqt::bridge {

// ── Construction ──────────────────────────────────────────────────────────────

DsContentTreeModel::DsContentTreeModel(QObject *parent)
    : QAbstractItemModel(parent)
    , m_root(new ContentTreeItem)
{
    // Rebuild whenever the bridge has finished updating. bridgeUpdated() is the
    // main-thread, "everything is settled" signal, so it is safe to touch the
    // model directly from the slot.
    connect(&DsQmlBridge::instance(), &DsQmlBridge::bridgeUpdated,
            this, &DsContentTreeModel::refresh);

    refresh();
}

DsContentTreeModel::~DsContentTreeModel()
{
    delete m_root;
}

// ── Tree construction ─────────────────────────────────────────────────────────

void DsContentTreeModel::refresh()
{
    const DatabaseContent &db = DsQmlBridge::instance().database();

    beginResetModel();

    delete m_root;
    m_root = new ContentTreeItem;

    auto makeGroup = [this](const QString &label) {
        auto *group     = new ContentTreeItem;
        group->label    = label;
        group->isGroup  = true;
        group->parent   = m_root;
        m_root->children.append(group);
        return group;
    };

    // Content and platforms are hierarchical (records reference children via
    // child_uid); events are shown as a flat list.
    buildGroup(makeGroup(QStringLiteral("Content")),   db.content(),   db, /*recurse*/ true);
    buildGroup(makeGroup(QStringLiteral("Events")),    db.events(),    db, /*recurse*/ false);
    buildGroup(makeGroup(QStringLiteral("Platforms")), db.platforms(), db, /*recurse*/ true);

    endResetModel();
}

void DsContentTreeModel::buildGroup(ContentTreeItem *group, const DatabaseRecordList &roots,
                                    const DatabaseContent &db, bool recurse)
{
    QSet<QString> visited;
    for (const DatabaseRecord &record : roots)
        addRecord(group, record, db, recurse, visited);
}

void DsContentTreeModel::addRecord(ContentTreeItem *parent, const DatabaseRecord &record,
                                   const DatabaseContent &db, bool recurse,
                                   QSet<QString> &visited)
{
    const QString uid = record.uid();
    // Guard against cycles / a record surfacing under more than one parent.
    if (!uid.isEmpty()) {
        if (visited.contains(uid))
            return;
        visited.insert(uid);
    }

    auto *item   = new ContentTreeItem;
    item->record = record;
    item->parent = parent;
    // Prefer the CMS record name; fall back to the type name, then the UID so a
    // node is never blank.
    item->label = record.recordName();
    if (item->label.isEmpty())
        item->label = record.typeName();
    if (item->label.isEmpty())
        item->label = uid;
    parent->children.append(item);

    if (!recurse)
        return;

    const DatabaseRecordList children = db.find(record.childUids());
    for (const DatabaseRecord &child : children)
        addRecord(item, child, db, recurse, visited);
}

// ── Index helpers ─────────────────────────────────────────────────────────────

ContentTreeItem *DsContentTreeModel::itemForIndex(const QModelIndex &index) const
{
    return index.isValid() ? static_cast<ContentTreeItem *>(index.internalPointer()) : m_root;
}

DatabaseRecord DsContentTreeModel::recordAt(const QModelIndex &index) const
{
    if (!index.isValid())
        return {};
    const auto *item = static_cast<ContentTreeItem *>(index.internalPointer());
    return item->isGroup ? DatabaseRecord{} : item->record;
}

QModelIndex DsContentTreeModel::indexForUid(const QString &uid) const
{
    if (uid.isEmpty())
        return {};

    // Depth-first walk that builds indexes as it descends.
    std::function<QModelIndex(const QModelIndex &)> search = [&](const QModelIndex &parent) {
        for (int row = 0; row < rowCount(parent); ++row) {
            const QModelIndex idx = index(row, 0, parent);
            const auto *item = static_cast<ContentTreeItem *>(idx.internalPointer());
            if (item && !item->isGroup && item->record.uid() == uid)
                return idx;
            if (const QModelIndex found = search(idx); found.isValid())
                return found;
        }
        return QModelIndex{};
    };
    return search(QModelIndex());
}

// ── QAbstractItemModel interface ──────────────────────────────────────────────

QModelIndex DsContentTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return {};
    const ContentTreeItem *parentItem = itemForIndex(parent);
    if (row < 0 || row >= parentItem->children.size())
        return {};
    return createIndex(row, column, parentItem->children.at(row));
}

QModelIndex DsContentTreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return {};
    const auto *item = static_cast<ContentTreeItem *>(index.internalPointer());
    ContentTreeItem *parentItem = item->parent;
    if (!parentItem || parentItem == m_root)
        return {};
    ContentTreeItem *grandParent = parentItem->parent ? parentItem->parent : m_root;
    const int row = grandParent->children.indexOf(parentItem);
    return createIndex(row, 0, parentItem);
}

int DsContentTreeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;
    const ContentTreeItem *item = itemForIndex(parent);
    return item->children.size();
}

int DsContentTreeModel::columnCount(const QModelIndex &) const
{
    return 2; // 0 = name, 1 = type
}

QVariant DsContentTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return {};
    const auto *item = static_cast<ContentTreeItem *>(index.internalPointer());

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case 0:
            // Group headers show their child count for quick orientation.
            return item->isGroup
                       ? QStringLiteral("%1 (%2)").arg(item->label).arg(item->children.size())
                       : item->label;
        case 1:
            return item->isGroup ? QString{} : item->record.typeName();
        }
        return {};
    case Qt::ToolTipRole:
        return item->isGroup ? QVariant{} : item->record.uid();
    case UidRole:
        return item->uid();
    case TypeRole:
        return item->isGroup ? QString{} : item->record.typeName();
    case IsGroupRole:
        return item->isGroup;
    case IsRecordRole:
        return !item->isGroup;
    case StableKeyRole:
        return item->stableKey();
    default:
        return {};
    }
}

QVariant DsContentTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case 0: return QStringLiteral("Name");
    case 1: return QStringLiteral("Type");
    }
    return {};
}

QHash<int, QByteArray> DsContentTreeModel::roleNames() const
{
    auto roles = QAbstractItemModel::roleNames();
    roles[UidRole]       = "uid";
    roles[TypeRole]      = "typeName";
    roles[IsGroupRole]   = "isGroup";
    roles[IsRecordRole]  = "isRecord";
    roles[StableKeyRole] = "stableKey";
    return roles;
}

} // namespace dsqt::bridge

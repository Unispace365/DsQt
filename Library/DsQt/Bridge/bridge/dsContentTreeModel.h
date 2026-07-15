#pragma once

#include "bridge/dsBridgeDatabase.h"

#include <QAbstractItemModel>
#include <QQmlEngine>
#include <QSet>

namespace dsqt::bridge {

// Internal tree node.
//
// A node is either a *group* header ("Content", "Events" or "Platforms") or a
// *record* node that wraps a single DatabaseRecord. Group nodes carry no record;
// record nodes carry a value-type copy of their DatabaseRecord so the detail
// panel can read every field without touching the (possibly since-replaced)
// live database.
struct ContentTreeItem
{
    QString        label;          // display name (group name or record name)
    DatabaseRecord record;         // the wrapped record — empty for group nodes
    bool           isGroup = false;

    ContentTreeItem              *parent = nullptr;
    QList<ContentTreeItem *>      children;

    ContentTreeItem() = default;
    ~ContentTreeItem() { qDeleteAll(children); }

    QString uid() const { return isGroup ? QString{} : record.uid(); }

    // A stable identifier used by the widget to remember which rows were
    // expanded/selected across a model reset. Records use their UID; groups use
    // their label with a prefix that cannot collide with a real UID.
    QString stableKey() const
    {
        return isGroup ? (QStringLiteral("\x01group:") + label) : record.uid();
    }
};

// A read-only QAbstractItemModel that exposes the bridge DatabaseContent as a
// three-section tree:
//
//   Content    (root content records and their descendants)
//   Events     (event records)
//   Platforms  (platform records and their descendants)
//
// The model uses *only* DatabaseRecord/DatabaseContent to read the data — it
// never touches ContentModel. It rebuilds itself automatically whenever the
// bridge finishes an update (DsQmlBridge::bridgeUpdated, delivered on the main
// thread). The rebuild goes through begin/endResetModel; DsContentBrowserWidget
// captures and restores expansion/selection state by UID around the reset so
// the user never loses their place.
class DsContentTreeModel : public QAbstractItemModel
{
    Q_OBJECT
    QML_ELEMENT

public:
    enum Roles {
        UidRole = Qt::UserRole + 1, // record UID (empty for groups)
        TypeRole,                   // content type display name
        IsGroupRole,                // true for the three section headers
        IsRecordRole,               // true for record nodes
        StableKeyRole,              // key used for expansion/selection restore
    };
    Q_ENUM(Roles)

    explicit DsContentTreeModel(QObject *parent = nullptr);
    ~DsContentTreeModel() override;

    // Rebuilds the tree from the current DsQmlBridge database. Called
    // automatically on bridgeUpdated(); can also be called manually.
    Q_INVOKABLE void refresh();

    // Returns the record wrapped by the given index, or an empty record for
    // group nodes / invalid indexes.
    DatabaseRecord recordAt(const QModelIndex &index) const;

    // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    void buildGroup(ContentTreeItem *group, const DatabaseRecordList &roots,
                    const DatabaseContent &db, bool recurse);
    void addRecord(ContentTreeItem *parent, const DatabaseRecord &record,
                   const DatabaseContent &db, bool recurse, QSet<QString> &visited);

    ContentTreeItem *itemForIndex(const QModelIndex &index) const;

    ContentTreeItem *m_root = nullptr;
};

} // namespace dsqt::bridge

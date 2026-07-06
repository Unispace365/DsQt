#pragma once

#include <QAbstractItemModel>
#include <QQmlEngine>
#include <QVariantMap>

namespace dsqt {

class SettingsFile;

// Internal tree node.
struct SettingsTreeItem
{
    QString  key;        // display name (last path segment)
    QString  fullPath;   // full dotted path (empty for root)
    QString  value;      // display string — non-empty for leaves only
    QString  typeName;   // friendly type name — non-empty for leaves only
    QString  provenance; // source file — non-empty for leaves only
    QVariant rawValue;   // original QVariant — used by setData() for type-aware parsing
    QVariantMap constraints; // optional schema metadata (min/max/step/options) — leaves only
    bool     isLeaf = false;
    bool hasOverride() const { return provenance == QStringLiteral("override"); }

    SettingsTreeItem *parent = nullptr;
    QList<SettingsTreeItem *> children;

    explicit SettingsTreeItem(const QString &key = {},
                               const QString &fullPath = {},
                               SettingsTreeItem *parent = nullptr)
        : key(key), fullPath(fullPath), parent(parent)
    {}

    ~SettingsTreeItem() { qDeleteAll(children); }
};

// A QAbstractItemModel that exposes a SettingsFile as a three-column tree.
// Column 0: key name (carries tree indentation).
// Column 1: value as a display string (leaves only).
// Column 2: friendly type name (leaves only).
// Additional roles: IsLeafRole, FullPathRole, ProvenanceRole, TypeRole,
// IsListRole, and ConstraintsRole, for use in QML delegates or item delegates.
class SettingsTreeModel : public QAbstractItemModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(SettingsFile *settingsFile READ settingsFile
                   WRITE setSettingsFile NOTIFY settingsFileChanged)

public:
    enum Roles {
        ValueRole       = Qt::UserRole + 1,
        IsLeafRole      = Qt::UserRole + 2,
        FullPathRole    = Qt::UserRole + 3,
        ProvenanceRole  = Qt::UserRole + 4,
        TypeRole        = Qt::UserRole + 5,
        IsListRole      = Qt::UserRole + 6,
        ConstraintsRole = Qt::UserRole + 7,
    };
    Q_ENUM(Roles)

    // Public so that ArrayEditorDialog (in SettingsViewerWidget.cpp) can reuse it.
    static QString displayString(const QVariant &v);

    explicit SettingsTreeModel(QObject *parent = nullptr);
    ~SettingsTreeModel() override;

    SettingsFile *settingsFile() const;
    void setSettingsFile(SettingsFile *sf);

    // Optional schema SettingsFile whose keys mirror settingsFile()'s dotted
    // paths, with each leaf holding a {min, max, step, options} metadata map
    // (any subset). Purely a viewer-side concern — never required, and safe
    // to leave unset (every lookup then yields an empty constraints map).
    void setSchema(SettingsFile *schema);

    // QAbstractItemModel interface
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value,
                 int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void settingsFileChanged();

private:
    void rebuild();
    void buildChildren(SettingsTreeItem *parent,
                       const QVariantMap &map,
                       const QString &prefix);
    void buildListChildren(SettingsTreeItem *parent,
                           const QVariantList &list,
                           const QString &prefix);

    SettingsFile *m_settingsFile = nullptr;
    SettingsFile *m_schema = nullptr;
    SettingsTreeItem *m_root = nullptr;
};

}

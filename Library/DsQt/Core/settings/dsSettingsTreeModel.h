#pragma once

#include <QAbstractItemModel>
#include <QQmlEngine>

namespace dsqt {

class SettingsFile;

// How the settings viewer edits one value: the setting's schema entry (see
// SettingsFile::schema()) interpreted against the type of the value it describes.
struct SettingsSchemaEntry
{
    enum class Kind {
        None,    // no usable schema: plain text editing
        Slider,  // numeric value with min and max
        Options, // value picked from a list
        File,    // string holding a file path: browse with a file dialog
        Folder,  // string holding a folder path: browse with a folder dialog
    };

    Kind      kind = Kind::None;
    QMetaType valueType;   // type of the setting the entry was built for
    QString   description; // tooltip text, may be empty
    bool      restart = false; // a change only takes effect after the app restarts

    // Slider
    double min      = 0.0;
    double max      = 0.0;
    double step     = 0.0;   // always > 0 for a slider
    bool   integral = false; // the setting holds an integer
    int    decimals = 0;     // digits kept after the point (0 when integral)

    // Options, already converted to valueType; labels has the same length.
    QVariantList options;
    QStringList  labels;

    // File and Folder
    QString fileFilter;          // QFileDialog name filter, e.g. "Images (*.png *.jpg)"
    bool    pathMissing = false; // the path (expanded) does not exist
    bool    detectPaths = true;  // false when the schema says editor = "text"

    // Builds the entry for a setting of `valueType` from its raw schema map.
    // Unusable parts are dropped: min/max on a non-numeric setting, options
    // that cannot be converted to valueType, a file/folder editor on anything
    // but a string. What remains decides the kind.
    static SettingsSchemaEntry fromMap(const QVariantMap &schema, QMetaType valueType);

    // Guesses whether a string without a schema editor holds a path: it must
    // start with a root path variable (%APP%, %LOCAL%, ...) or a drive letter
    // or UNC prefix, or be a "/"-rooted path that exists. An existing path is
    // a File or Folder as found on disk; otherwise a trailing slash or a
    // missing extension means Folder. Anything else (URLs, plain words,
    // relative paths) is Kind::None.
    static Kind detectPath(const QString &value);

    // Slider: `v` clamped to [min, max] and snapped to the step grid, as valueType.
    QVariant snapped(double v) const;
    // Slider: `v` clamped to [min, max] but not snapped (for exact typed values).
    QVariant clamped(double v) const;
    // Slider: position of `v` along the range, 0.0 to 1.0.
    double ratio(double v) const;
    // Slider: `v` formatted with the entry's precision.
    QString format(double v) const;
    // Options: index of `value` in options, or -1.
    int optionIndex(const QVariant &value) const;
};

// Internal tree node.
struct SettingsTreeItem
{
    QString  key;        // display name (last path segment)
    QString  fullPath;   // full dotted path (empty for root)
    QString  value;      // display string — non-empty for leaves only
    QString  typeName;   // friendly type name — non-empty for leaves only
    QString  provenance; // source file — non-empty for leaves only
    QVariant rawValue;   // original QVariant — used by setData() for type-aware parsing
    QVariantMap schemaMap;      // raw schema entry from SettingsFile::schema()
    SettingsSchemaEntry schema; // schemaMap interpreted for rawValue's type
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
// Additional roles: IsLeafRole and FullPathRole for use in QML delegates.
class SettingsTreeModel : public QAbstractItemModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(SettingsFile *settingsFile READ settingsFile
                   WRITE setSettingsFile NOTIFY settingsFileChanged)

public:
    enum Roles {
        ValueRole      = Qt::UserRole + 1,
        IsLeafRole     = Qt::UserRole + 2,
        FullPathRole   = Qt::UserRole + 3,
        ProvenanceRole = Qt::UserRole + 4,
        TypeRole       = Qt::UserRole + 5,
        IsListRole     = Qt::UserRole + 6,
        SchemaRole     = Qt::UserRole + 7, // raw schema map (QVariantMap), for QML
        SchemaEntryRole = Qt::UserRole + 8, // SettingsSchemaEntry, for the widget delegate
    };
    Q_ENUM(Roles)

    // Public so that ArrayEditorDialog (in SettingsViewerWidget.cpp) can reuse them.
    static QString displayString(const QVariant &v);
    // Inverse of displayString(): parses text typed by the user back into a value
    // of `type`. Lenient about separators, so "(10, 20)", "10, 20", "10 20" and
    // "10 × 20" all parse as two numbers. Returns an invalid QVariant if the text
    // cannot be represented as `type`.
    static QVariant parseDisplayString(const QString &text, QMetaType type);

    explicit SettingsTreeModel(QObject *parent = nullptr);
    ~SettingsTreeModel() override;

    SettingsFile *settingsFile() const;
    void setSettingsFile(SettingsFile *sf);

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
    // Rebuilds the tree from the settings file. When only values changed (same
    // keys in the same places) the existing rows are updated in place and
    // dataChanged is emitted, so views keep their expansion state, selection
    // and open editors; a slider can be dragged without its row being reset.
    // Any structural change, or `forceReset`, resets the model instead.
    void rebuild(bool forceReset = false);
    void updateInPlace(SettingsTreeItem *current,
                       const SettingsTreeItem *fresh,
                       const QModelIndex &parentIndex);
    // `restart`: an enclosing table's schema says restart = true, which
    // applies to every setting below it.
    void buildChildren(SettingsTreeItem *parent,
                       const QVariantMap &map,
                       const QString &prefix,
                       bool restart = false);
    void buildListChildren(SettingsTreeItem *parent,
                           const QVariantList &list,
                           const QString &prefix,
                           bool restart = false);
    bool schemaSaysRestart(const QString &path) const;

    SettingsFile *m_settingsFile = nullptr;
    SettingsTreeItem *m_root = nullptr;
};

}

Q_DECLARE_METATYPE(dsqt::SettingsSchemaEntry)

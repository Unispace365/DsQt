#ifndef DSSETTINGSTREEMODEL_H
#define DSSETTINGSTREEMODEL_H

#include "settings/dsSettings.h"
#include <QAbstractItemModel>
#include <qqml.h>

namespace dsqt {

class SettingsTreeItem {
  public:
    SettingsTreeItem() = default;
    SettingsTreeItem(const QString& key, SettingsTreeItem* parent = nullptr);

    SettingsTreeItem* parentItem() { return m_parentItem; }
    SettingsTreeItem* child(int row);
    int               childCount() const;
    int               columnCount() const { return 1; }
    QVariant          data(int role) const;
    int               row() const;

    void addChild(std::shared_ptr<SettingsTreeItem> child);

    QString m_key;
    QString m_fullKeyPath;
    QString m_value;
    QString m_source;
    QString m_type;
    bool    m_isLeaf = false;

  private:
    SettingsTreeItem*                              m_parentItem = nullptr;
    std::vector<std::shared_ptr<SettingsTreeItem>> m_children;
};

class DsSettingsTreeModel : public QAbstractItemModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(DsSettingsTreeModel)
    Q_PROPERTY(QString settingsName READ settingsName WRITE setSettingsName NOTIFY settingsNameChanged)
    Q_PROPERTY(DsSettings* settings READ settings WRITE setSettings NOTIFY settingsChanged)
    Q_PROPERTY(QString filterFile READ filterFile WRITE setFilterFile NOTIFY filterFileChanged)
    Q_PROPERTY(QStringList loadedFiles READ loadedFiles NOTIFY loadedFilesChanged)
    Q_PROPERTY(QStringList availableSettings READ availableSettings NOTIFY availableSettingsChanged)

  public:
    explicit DsSettingsTreeModel(QObject* parent = nullptr);
    ~DsSettingsTreeModel() override = default;

    QModelIndex            index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex            parent(const QModelIndex& index) const override;
    int                    rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int                    columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant               data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags          flags(const QModelIndex& index) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void reload();
    Q_INVOKABLE QVariantList search(const QString& text) const;
    Q_INVOKABLE QModelIndex  indexForPath(const QString& dottedPath) const;

    QString      settingsName() const { return m_settingsName; }
    void         setSettingsName(const QString& name);
    DsSettings*  settings() const { return m_settings.get(); }
    void         setSettings(DsSettings* settings);
    QString      filterFile() const { return m_filterFile; }
    void         setFilterFile(const QString& file);
    QStringList  loadedFiles() const { return m_loadedFiles; }
    QStringList  availableSettings() const { return DsSettings::getSettingsNames(); }

  signals:
    void settingsNameChanged();
    void settingsChanged();
    void filterFileChanged();
    void loadedFilesChanged();
    void availableSettingsChanged();

  private:
    void rebuild();
    void buildTree(const QVariantMap& map, SettingsTreeItem* parent, const QString& parentPath);
    void searchRecursive(SettingsTreeItem* item, const QString& text, QVariantList& results) const;

    DsSettingsRef                     m_settings;
    QString                           m_settingsName;
    QString                           m_filterFile;
    QStringList                       m_loadedFiles;
    std::shared_ptr<SettingsTreeItem> m_rootItem;
};

} // namespace dsqt
#endif // DSSETTINGSTREEMODEL_H

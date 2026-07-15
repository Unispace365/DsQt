#pragma once

#include "bridge/dsBridgeDatabase.h"

#include <QSet>
#include <QString>
#include <QWidget>

class QLineEdit;
class QModelIndex;
class QTableWidget;
class QTreeView;

namespace dsqt::bridge {

class DsContentTreeModel;
class ContentFilterProxyModel;
class MediaPreview;

// A Qt Widgets window that browses the bridge database.
//
// Layout:
//   ┌───────────────────────────────────────────────┐
//   │ [ filter box ]                                 │
//   ├───────────────┬───────────────────────────────┤
//   │  tree view    │  field table (key / value)     │
//   │  Content      │───────────────────────────────│
//   │  Events       │  media preview (resources)     │
//   │  Platforms    │                                │
//   └───────────────┴───────────────────────────────┘
//
// It reads content exclusively through DatabaseRecord / DatabaseContent (never
// ContentModel) via DsContentTreeModel. The model rebuilds itself when the
// bridge updates; this widget captures the set of expanded rows and the current
// selection (keyed by record UID) before each rebuild and restores them
// afterwards, so live database updates no longer collapse the tree or lose the
// user's place.
class DsContentBrowserWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DsContentBrowserWidget(QWidget *parent = nullptr);
    ~DsContentBrowserWidget() override;

signals:
    // Fired whenever the widget is shown or hidden (including via the window's
    // own close button), so a menu checkbox can stay in sync.
    void visibilityChanged(bool visible);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    // Selection / detail handling.
    void onCurrentRecordChanged(const QModelIndex &current);
    void populateFields(const DatabaseRecord &record);
    void onFieldActivated(int row);

    // Expansion / selection preservation across model resets.
    void captureViewState();
    void restoreViewState();
    void collectExpanded(const QModelIndex &parent, QSet<QString> &out) const;
    void expandMatching(const QModelIndex &parent, const QSet<QString> &keys);
    QModelIndex indexForKey(const QModelIndex &parent, const QString &key) const;

    DsContentTreeModel      *m_model;
    ContentFilterProxyModel *m_proxy;
    QTreeView               *m_tree;
    QLineEdit               *m_filter;
    QTableWidget            *m_fields;
    MediaPreview            *m_preview;

    // Snapshot of the record whose fields are currently shown, so field-row
    // clicks can resolve resources without another lookup.
    DatabaseRecord m_currentRecord;

    // Captured view state (stable keys) used to restore expansion/selection.
    QSet<QString> m_savedExpanded;
    QString       m_savedSelected;
};

} // namespace dsqt::bridge

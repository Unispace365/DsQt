#pragma once

#include "bridge/dsBridgeDatabase.h"

#include <QSet>
#include <QString>
#include <QWidget>

class QAction;
class QLineEdit;
class QModelIndex;
class QTableWidget;
class QToolButton;
class QTreeView;

namespace dsqt::bridge {

class DsContentTreeModel;
class ContentFilterProxyModel;
class MediaPreview;
class UidPillDelegate;

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

    // Returns true if the field should be shown given the current view options.
    bool fieldVisible(const QString &key) const;
    // Shows or hides the media pane according to the Auto-Hide Media option.
    void applyMediaVisibility();

    // UID pills: split a field into its UID tokens (empty unless at least one
    // token is a known record UID), and jump the tree selection to a given UID.
    QStringList uidTokensFor(const DatabaseRecord &record, const QString &key) const;
    void navigateToUid(const QString &uid);

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
    QToolButton             *m_options;
    QTableWidget            *m_fields;
    MediaPreview            *m_preview;
    UidPillDelegate         *m_pillDelegate;

    // View-option toggles (settings/cog popup, right of the filter box).
    QAction *m_showFieldUids;   // show "*_field_uid" fields          (off by default)
    QAction *m_showCommonFields; // show rank/type_*/variant fields   (on by default)
    QAction *m_autoHideMedia;    // hide media pane when nothing shown (on by default)

    // Snapshot of the record whose fields are currently shown, so field-row
    // clicks can resolve resources without another lookup.
    DatabaseRecord m_currentRecord;

    // Whether the preview currently shows a media resource — drives Auto-Hide.
    bool m_hasMedia = false;

    // Captured view state (stable keys) used to restore expansion/selection.
    QSet<QString> m_savedExpanded;
    QString       m_savedSelected;
};

} // namespace dsqt::bridge

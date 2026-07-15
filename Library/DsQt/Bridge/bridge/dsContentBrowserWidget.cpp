#include "bridge/dsContentBrowserWidget.h"
#include "bridge/dsContentTreeModel.h"
#include "bridge/dsQmlBridge.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QAudioOutput>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QImage>
#include <QImageReader>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMediaPlayer>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QRect>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QToolButton>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>
#include <QVideoWidget>

#include <algorithm>
#include <functional>

namespace dsqt::bridge {

// ── ContentFilterProxyModel ───────────────────────────────────────────────────
// Case-insensitive substring filter over a row's name (column 0), type
// (column 1) and UID. A row stays visible if it matches directly or if any
// descendant matches, so the path down to a match is never hidden.

class ContentFilterProxyModel : public QSortFilterProxyModel
{
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    void setFilterKeyword(const QString &text)
    {
        if (m_keyword == text)
            return;
        m_keyword = text;
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
    {
        if (m_keyword.isEmpty())
            return true;

        QAbstractItemModel *model = sourceModel();
        const QModelIndex index = model->index(sourceRow, 0, sourceParent);
        if (rowMatches(index))
            return true;

        for (int i = 0; i < model->rowCount(index); ++i) {
            if (filterAcceptsRow(i, index))
                return true;
        }
        return false;
    }

private:
    bool rowMatches(const QModelIndex &index) const
    {
        const QString name = index.data(Qt::DisplayRole).toString();
        const QString type = index.siblingAtColumn(1).data(Qt::DisplayRole).toString();
        const QString uid  = index.data(DsContentTreeModel::UidRole).toString();
        return name.contains(m_keyword, Qt::CaseInsensitive)
            || type.contains(m_keyword, Qt::CaseInsensitive)
            || uid.contains(m_keyword, Qt::CaseInsensitive);
    }

    QString m_keyword;
};

// ── MediaPreview ──────────────────────────────────────────────────────────────
// Shows a DatabaseResource: images as a (cropped) scaled pixmap, videos through
// a looping QMediaPlayer/QVideoWidget, and everything else (PDF, WEB, invalid)
// as a short info line.

class MediaPreview : public QWidget
{
public:
    explicit MediaPreview(QWidget *parent = nullptr)
        : QWidget(parent)
        , m_stack(new QStackedWidget(this))
        , m_imageLabel(new QLabel)
        , m_video(new QVideoWidget)
        , m_info(new QLabel)
        , m_player(new QMediaPlayer(this))
        , m_audio(new QAudioOutput(this))
    {
        m_imageLabel->setAlignment(Qt::AlignCenter);
        m_imageLabel->setMinimumSize(1, 1);
        m_info->setAlignment(Qt::AlignCenter);
        m_info->setWordWrap(true);
        m_info->setTextInteractionFlags(Qt::TextSelectableByMouse);

        m_player->setVideoOutput(m_video);
        m_player->setAudioOutput(m_audio);
        m_player->setLoops(QMediaPlayer::Infinite);

        m_stack->addWidget(m_imageLabel); // index 0
        m_stack->addWidget(m_video);      // index 1
        m_stack->addWidget(m_info);       // index 2

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_stack);

        clear();
    }

    ~MediaPreview() override { m_player->stop(); }

    void clear()
    {
        m_player->stop();
        m_sourcePixmap = QPixmap();
        m_imageLabel->clear();
        m_info->setText(tr("Select a media field to preview."));
        m_stack->setCurrentWidget(m_info);
    }

    void show(const DatabaseResource &res)
    {
        m_player->stop();

        if (!res.isValid()) {
            clear();
            return;
        }

        const QString type = res.type().toUpper();
        const QString path = res.filepath();

        if (type == QLatin1String("IMAGE")) {
            QImageReader reader(path);
            reader.setAutoTransform(true);
            QImage image = reader.read();
            if (image.isNull()) {
                showInfo(tr("Could not load image:\n%1").arg(path));
                return;
            }
            m_sourcePixmap = QPixmap::fromImage(cropped(image, res.crop()));
            m_stack->setCurrentWidget(m_imageLabel);
            rescale();
        } else if (type == QLatin1String("VIDEO")) {
            m_sourcePixmap = QPixmap();
            m_stack->setCurrentWidget(m_video);
            m_player->setSource(QUrl::fromLocalFile(path));
            m_player->play();
        } else {
            // PDF, WEB, or anything without a dedicated preview.
            showInfo(QStringLiteral("[%1]\n%2").arg(type, path));
        }
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        if (m_stack->currentWidget() == m_imageLabel)
            rescale();
    }

private:
    void showInfo(const QString &text)
    {
        m_sourcePixmap = QPixmap();
        m_info->setText(text);
        m_stack->setCurrentWidget(m_info);
    }

    // Applies a normalised [0-1] crop rectangle to an image.
    static QImage cropped(const QImage &image, const QRectF &crop)
    {
        if (crop == QRectF(0, 0, 1, 1) || image.isNull())
            return image;
        const QRect r(qRound(crop.x() * image.width()),
                      qRound(crop.y() * image.height()),
                      qRound(crop.width() * image.width()),
                      qRound(crop.height() * image.height()));
        const QRect clamped = r.intersected(image.rect());
        return clamped.isEmpty() ? image : image.copy(clamped);
    }

    void rescale()
    {
        if (m_sourcePixmap.isNull()) {
            m_imageLabel->clear();
            return;
        }
        m_imageLabel->setPixmap(m_sourcePixmap.scaled(
            m_imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    QStackedWidget *m_stack;
    QLabel         *m_imageLabel;
    QVideoWidget   *m_video;
    QLabel         *m_info;
    QMediaPlayer   *m_player;
    QAudioOutput   *m_audio;
    QPixmap         m_sourcePixmap; // cropped, unscaled source for the image page
};

// ── Field value formatting ────────────────────────────────────────────────────

static QString formatFieldValue(const DatabaseRecord &record, const QString &key)
{
    if (record.isResource(key)) {
        const DatabaseResource res = record.resource(key);
        return QStringLiteral("[%1] %2").arg(res.type(), res.filepath());
    }

    const QVariant v = record.value(key);
    if (v.metaType() == QMetaType::fromType<QStringList>())
        return v.toStringList().join(QStringLiteral(", "));
    if (v.metaType() == QMetaType::fromType<QVariantList>()) {
        QStringList parts;
        const QVariantList list = v.toList();
        parts.reserve(list.size());
        for (const QVariant &item : list)
            parts.append(item.toString());
        return parts.join(QStringLiteral(", "));
    }
    return v.toString();
}

// Item-data role (column 1) holding the ordered list of tokens to render as
// pills. Present only on cells that contain at least one known UID.
static constexpr int kPillTokensRole = Qt::UserRole + 1;

// ── UidPillDelegate ───────────────────────────────────────────────────────────
// Paints a value cell's UID tokens as rounded "pills" (unknown tokens are drawn
// as plain text) and reports clicks on a pill through the onUidClicked callback.
// A token is a pill when the current bridge database contains a record with that
// UID, so it always reflects the live database.

class UidPillDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    std::function<void(const QString &)> onUidClicked;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        const QStringList tokens = index.data(kPillTokensRole).toStringList();
        if (tokens.isEmpty()) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        // Draw the standard item chrome (selection, alternating background) but
        // suppress the text — we render our own content on top.
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        opt.text.clear();
        const QWidget *w = opt.widget;
        QStyle *style = w ? w->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, w);

        const bool selected = option.state & QStyle::State_Selected;
        painter->save();
        painter->setClipRect(option.rect);
        painter->setRenderHint(QPainter::Antialiasing, true);
        for (const Pill &pill : layoutPills(option, tokens)) {
            if (pill.isUid) {
                QColor bg = option.palette.color(QPalette::Highlight);
                bg.setAlpha(selected ? 200 : 70);
                painter->setPen(Qt::NoPen);
                painter->setBrush(bg);
                const qreal r = pill.rect.height() / 2.0;
                painter->drawRoundedRect(pill.rect, r, r);
                painter->setPen(selected ? option.palette.color(QPalette::HighlightedText)
                                         : option.palette.color(QPalette::Text));
                painter->drawText(pill.rect, Qt::AlignCenter, pill.token);
            } else {
                painter->setPen(option.palette.color(
                    selected ? QPalette::HighlightedText : QPalette::Text));
                painter->drawText(pill.rect, Qt::AlignVCenter | Qt::AlignLeft, pill.token);
            }
        }
        painter->restore();
    }

    bool editorEvent(QEvent *event, QAbstractItemModel *model,
                     const QStyleOptionViewItem &option, const QModelIndex &index) override
    {
        if (event->type() == QEvent::MouseButtonRelease) {
            const QStringList tokens = index.data(kPillTokensRole).toStringList();
            if (!tokens.isEmpty()) {
                auto *me = static_cast<QMouseEvent *>(event);
                for (const Pill &pill : layoutPills(option, tokens)) {
                    if (pill.isUid && pill.rect.contains(me->pos())) {
                        if (onUidClicked)
                            onUidClicked(pill.token);
                        return true;
                    }
                }
            }
        }
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }

private:
    struct Pill { QRect rect; QString token; bool isUid; };

    QList<Pill> layoutPills(const QStyleOptionViewItem &option, const QStringList &tokens) const
    {
        constexpr int hpad = 8, spacing = 4;
        const DatabaseRecordHash &records = DsQmlBridge::instance().database().records();
        const QFontMetrics fm(option.font);
        const int h   = qMax(fm.height() + 2, option.rect.height() - 6);
        const int top = option.rect.top() + (option.rect.height() - h) / 2;
        int x = option.rect.left() + 4;

        QList<Pill> pills;
        for (const QString &raw : tokens) {
            const QString token = raw.trimmed();
            if (token.isEmpty())
                continue;
            const bool isUid = records.contains(token);
            const int textW  = fm.horizontalAdvance(token);
            const int w      = isUid ? textW + 2 * hpad : textW;
            pills.append({QRect(x, top, w, h), token, isUid});
            x += w + spacing;
        }
        return pills;
    }
};

// ── DsContentBrowserWidget ────────────────────────────────────────────────────

DsContentBrowserWidget::DsContentBrowserWidget(QWidget *parent)
    : QWidget(parent, Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint
                          | Qt::WindowCloseButtonHint | Qt::WindowMaximizeButtonHint)
    , m_model(new DsContentTreeModel(this))
    , m_proxy(new ContentFilterProxyModel(this))
    , m_tree(new QTreeView)
    , m_filter(new QLineEdit)
    , m_options(new QToolButton)
    , m_fields(new QTableWidget)
    , m_preview(new MediaPreview)
    , m_pillDelegate(new UidPillDelegate(this))
{
    setWindowTitle(tr("Content Browser"));
    resize(1200, 700);

    m_proxy->setSourceModel(m_model);

    // ── Tree ──
    m_tree->setModel(m_proxy);
    m_tree->setUniformRowHeights(true);
    m_tree->setAlternatingRowColors(true);
    m_tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_tree->header()->resizeSection(0, 320);
    m_tree->header()->setStretchLastSection(true);
    m_tree->expandToDepth(0); // show the three section headers expanded

    // ── Filter ──
    m_filter->setPlaceholderText(tr("Filter by name, type, or UID…"));
    m_filter->setClearButtonEnabled(true);

    // ── View-options (cog) popup, to the right of the filter box ──
    auto *optionsMenu = new QMenu(this);
    m_showFieldUids = optionsMenu->addAction(tr("Show Field UIDs"));
    m_showCommonFields = optionsMenu->addAction(tr("Show Common Fields"));
    m_autoHideMedia = optionsMenu->addAction(tr("Auto-Hide Media"));
    for (QAction *a : {m_showFieldUids, m_showCommonFields, m_autoHideMedia})
        a->setCheckable(true);
    m_showFieldUids->setChecked(false);   // hide "*_field_uid" fields by default
    m_showCommonFields->setChecked(true); // show rank/type_*/variant by default
    m_autoHideMedia->setChecked(true);    // hide the media pane when nothing is shown

    m_options->setText(QStringLiteral("⚙")); // ⚙ gear glyph
    m_options->setToolTip(tr("View options"));
    m_options->setPopupMode(QToolButton::InstantPopup);
    m_options->setAutoRaise(true);
    m_options->setMenu(optionsMenu);

    // ── Field table ──
    m_fields->setColumnCount(2);
    m_fields->setHorizontalHeaderLabels({tr("Field"), tr("Value")});
    m_fields->verticalHeader()->setVisible(false);
    m_fields->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_fields->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fields->setSelectionMode(QAbstractItemView::SingleSelection);
    m_fields->setAlternatingRowColors(true); // match the tree
    m_fields->horizontalHeader()->resizeSection(0, 200);
    m_fields->horizontalHeader()->setStretchLastSection(true);
    m_fields->setWordWrap(false);

    // Give the field rows the same height as the tree rows. sizeHintForRow() is
    // valid here because the model already holds the three section rows.
    const int rowHeight = m_tree->sizeHintForRow(0);
    if (rowHeight > 0) {
        QHeaderView *vh = m_fields->verticalHeader();
        vh->setSectionResizeMode(QHeaderView::Fixed);
        vh->setDefaultSectionSize(rowHeight);
    }

    // Render UID-bearing value cells as clickable pills that jump to the record.
    m_pillDelegate->onUidClicked = [this](const QString &uid) { navigateToUid(uid); };
    m_fields->setItemDelegateForColumn(1, m_pillDelegate);

    // ── Right side: fields over preview ──
    auto *rightSplit = new QSplitter(Qt::Vertical);
    rightSplit->addWidget(m_fields);
    rightSplit->addWidget(m_preview);
    rightSplit->setStretchFactor(0, 1);
    rightSplit->setStretchFactor(1, 2);

    auto *mainSplit = new QSplitter(Qt::Horizontal);
    mainSplit->addWidget(m_tree);
    mainSplit->addWidget(rightSplit);
    mainSplit->setStretchFactor(0, 2);
    mainSplit->setStretchFactor(1, 3);

    auto *topRow = new QHBoxLayout;
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->addWidget(m_filter, 1);
    topRow->addWidget(m_options);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);
    layout->addLayout(topRow);
    layout->addWidget(mainSplit, 1);

    // ── Wiring ──
    connect(m_filter, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_proxy->setFilterKeyword(text);
        if (!text.isEmpty())
            m_tree->expandAll(); // reveal matches under collapsed branches
    });

    connect(m_tree->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, [this](const QModelIndex &current, const QModelIndex &) {
                onCurrentRecordChanged(current);
            });

    connect(m_fields, &QTableWidget::cellClicked, this,
            [this](int row, int) { onFieldActivated(row); });

    // View-option toggles: field visibility re-runs the field list; auto-hide
    // just re-evaluates the media pane's visibility.
    connect(m_showFieldUids, &QAction::toggled, this,
            [this] { populateFields(m_currentRecord); });
    connect(m_showCommonFields, &QAction::toggled, this,
            [this] { populateFields(m_currentRecord); });
    connect(m_autoHideMedia, &QAction::toggled, this,
            [this] { applyMediaVisibility(); });
    applyMediaVisibility(); // apply the default (no media shown yet)

    // Preserve expansion/selection across the model's automatic rebuilds.
    // Capture runs synchronously while the pre-reset tree is still valid.
    // Restore is queued so it runs only after the reset has fully settled —
    // this keeps the heavier restore work (expansion, re-selection, and the
    // field/media repopulation it triggers) out of the model-reset signal,
    // avoiding re-entrant resets that could corrupt the model.
    connect(m_proxy, &QAbstractItemModel::modelAboutToBeReset,
            this, &DsContentBrowserWidget::captureViewState);
    connect(m_proxy, &QAbstractItemModel::modelReset,
            this, &DsContentBrowserWidget::restoreViewState, Qt::QueuedConnection);
}

DsContentBrowserWidget::~DsContentBrowserWidget() = default;

void DsContentBrowserWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    emit visibilityChanged(true);
}

void DsContentBrowserWidget::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    emit visibilityChanged(false);
}

// ── Selection / detail ────────────────────────────────────────────────────────

void DsContentBrowserWidget::onCurrentRecordChanged(const QModelIndex &current)
{
    const QModelIndex source = m_proxy->mapToSource(current);
    m_currentRecord = m_model->recordAt(source);
    populateFields(m_currentRecord);
}

void DsContentBrowserWidget::populateFields(const DatabaseRecord &record)
{
    m_fields->clearContents();

    // Sort keys, then drop the ones hidden by the current view options.
    QStringList keys = record.keys();
    std::sort(keys.begin(), keys.end(), [](const QString &a, const QString &b) {
        return a.localeAwareCompare(b) < 0;
    });
    keys.removeIf([this](const QString &key) { return !fieldVisible(key); });

    m_fields->setRowCount(keys.size());
    QString firstResourceKey;
    for (int row = 0; row < keys.size(); ++row) {
        const QString &key = keys.at(row);

        auto *keyItem = new QTableWidgetItem(key);
        auto *valItem = new QTableWidgetItem(formatFieldValue(record, key));
        if (record.isResource(key)) {
            // Tag the row so a click can load the resource into the preview.
            keyItem->setData(Qt::UserRole, key);
            valItem->setData(Qt::UserRole, key);
            if (firstResourceKey.isEmpty())
                firstResourceKey = key;
        } else {
            // If the field holds one or more known UIDs, tag it so the delegate
            // renders those tokens as clickable pills.
            const QStringList tokens = uidTokensFor(record, key);
            if (!tokens.isEmpty())
                valItem->setData(kPillTokensRole, tokens);
        }
        m_fields->setItem(row, 0, keyItem);
        m_fields->setItem(row, 1, valItem);
    }

    // Auto-preview the record's first media field, if any.
    if (!firstResourceKey.isEmpty()) {
        m_preview->show(record.resource(firstResourceKey));
        m_hasMedia = true;
    } else {
        m_preview->clear();
        m_hasMedia = false;
    }
    applyMediaVisibility();
}

void DsContentBrowserWidget::onFieldActivated(int row)
{
    if (row < 0)
        return;
    QTableWidgetItem *item = m_fields->item(row, 0);
    if (!item)
        return;
    const QString key = item->data(Qt::UserRole).toString();
    if (key.isEmpty())
        return; // not a resource field
    m_preview->show(m_currentRecord.resource(key));
    m_hasMedia = true;
    applyMediaVisibility();
}

// ── View options ──────────────────────────────────────────────────────────────

bool DsContentBrowserWidget::fieldVisible(const QString &key) const
{
    if (!m_showFieldUids->isChecked() && key.endsWith(QLatin1String("_field_uid")))
        return false;

    if (!m_showCommonFields->isChecked()) {
        static const QSet<QString> kCommon = {
            QStringLiteral("child_uid"),   QStringLiteral("label"),
            QStringLiteral("parent_slot"), QStringLiteral("parent_uid"),
            QStringLiteral("rank"),        QStringLiteral("reverse_ordered"),
            QStringLiteral("record_name"),
            QStringLiteral("type_key"),    QStringLiteral("type_name"),
            QStringLiteral("type_uid"),    QStringLiteral("variant"),
            QStringLiteral("uid"),
        };
        if (kCommon.contains(key))
            return false;
    }
    return true;
}

void DsContentBrowserWidget::applyMediaVisibility()
{
    m_preview->setVisible(!m_autoHideMedia->isChecked() || m_hasMedia);
}

// ── UID pills / navigation ────────────────────────────────────────────────────

QStringList DsContentBrowserWidget::uidTokensFor(const DatabaseRecord &record,
                                                 const QString &key) const
{
    const QVariant v = record.value(key);

    QStringList tokens;
    if (v.metaType() == QMetaType::fromType<QStringList>()) {
        tokens = v.toStringList();
    } else if (v.metaType() == QMetaType::fromType<QVariantList>()) {
        const QVariantList list = v.toList();
        tokens.reserve(list.size());
        for (const QVariant &item : list)
            tokens.append(item.toString());
    } else {
        // A scalar field may still hold a single UID, or a comma-separated list.
        tokens = v.toString().split(QLatin1Char(','), Qt::SkipEmptyParts);
    }

    // Only treat this as a UID field if at least one token is a known record.
    const DatabaseRecordHash &records = DsQmlBridge::instance().database().records();
    bool anyKnown = false;
    for (const QString &token : tokens) {
        if (records.contains(token.trimmed())) {
            anyKnown = true;
            break;
        }
    }
    return anyKnown ? tokens : QStringList{};
}

void DsContentBrowserWidget::navigateToUid(const QString &uid)
{
    const QModelIndex source = m_model->indexForUid(uid);
    if (!source.isValid())
        return;

    // Clear any active filter so the target record can't be hidden.
    if (!m_filter->text().isEmpty())
        m_filter->clear();

    const QModelIndex target = m_proxy->mapFromSource(source);
    if (!target.isValid())
        return;

    for (QModelIndex p = target.parent(); p.isValid(); p = p.parent())
        m_tree->expand(p);

    m_tree->setCurrentIndex(target); // triggers the field panel to repopulate
    m_tree->scrollTo(target, QAbstractItemView::PositionAtCenter);
    m_tree->setFocus();
}

// ── Expansion / selection preservation ────────────────────────────────────────

void DsContentBrowserWidget::captureViewState()
{
    m_savedExpanded.clear();
    collectExpanded(QModelIndex(), m_savedExpanded);

    const QModelIndex current = m_tree->currentIndex();
    m_savedSelected = current.isValid()
                          ? current.data(DsContentTreeModel::StableKeyRole).toString()
                          : QString{};
}

void DsContentBrowserWidget::restoreViewState()
{
    expandMatching(QModelIndex(), m_savedExpanded);

    if (!m_savedSelected.isEmpty()) {
        const QModelIndex idx = indexForKey(QModelIndex(), m_savedSelected);
        if (idx.isValid()) {
            m_tree->setCurrentIndex(idx);
            m_tree->scrollTo(idx);
            return;
        }
    }
    // Nothing to reselect (or the record is gone): fall back to expanded groups.
    if (m_savedExpanded.isEmpty())
        m_tree->expandToDepth(0);
}

void DsContentBrowserWidget::collectExpanded(const QModelIndex &parent, QSet<QString> &out) const
{
    const int rows = m_proxy->rowCount(parent);
    for (int i = 0; i < rows; ++i) {
        const QModelIndex idx = m_proxy->index(i, 0, parent);
        if (m_tree->isExpanded(idx))
            out.insert(idx.data(DsContentTreeModel::StableKeyRole).toString());
        collectExpanded(idx, out);
    }
}

void DsContentBrowserWidget::expandMatching(const QModelIndex &parent, const QSet<QString> &keys)
{
    const int rows = m_proxy->rowCount(parent);
    for (int i = 0; i < rows; ++i) {
        const QModelIndex idx = m_proxy->index(i, 0, parent);
        if (keys.contains(idx.data(DsContentTreeModel::StableKeyRole).toString()))
            m_tree->expand(idx);
        expandMatching(idx, keys);
    }
}

QModelIndex DsContentBrowserWidget::indexForKey(const QModelIndex &parent, const QString &key) const
{
    const int rows = m_proxy->rowCount(parent);
    for (int i = 0; i < rows; ++i) {
        const QModelIndex idx = m_proxy->index(i, 0, parent);
        if (idx.data(DsContentTreeModel::StableKeyRole).toString() == key)
            return idx;
        const QModelIndex found = indexForKey(idx, key);
        if (found.isValid())
            return found;
    }
    return {};
}

} // namespace dsqt::bridge

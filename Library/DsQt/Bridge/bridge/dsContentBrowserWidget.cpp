#include "bridge/dsContentBrowserWidget.h"
#include "bridge/dsContentTreeModel.h"

#include <QAudioOutput>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QImage>
#include <QImageReader>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMediaPlayer>
#include <QPixmap>
#include <QRect>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>
#include <QVideoWidget>

#include <algorithm>

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

// ── DsContentBrowserWidget ────────────────────────────────────────────────────

DsContentBrowserWidget::DsContentBrowserWidget(QWidget *parent)
    : QWidget(parent, Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint
                          | Qt::WindowCloseButtonHint | Qt::WindowMaximizeButtonHint)
    , m_model(new DsContentTreeModel(this))
    , m_proxy(new ContentFilterProxyModel(this))
    , m_tree(new QTreeView)
    , m_filter(new QLineEdit)
    , m_fields(new QTableWidget)
    , m_preview(new MediaPreview)
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

    // ── Field table ──
    m_fields->setColumnCount(2);
    m_fields->setHorizontalHeaderLabels({tr("Field"), tr("Value")});
    m_fields->verticalHeader()->setVisible(false);
    m_fields->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_fields->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fields->setSelectionMode(QAbstractItemView::SingleSelection);
    m_fields->horizontalHeader()->resizeSection(0, 200);
    m_fields->horizontalHeader()->setStretchLastSection(true);
    m_fields->setWordWrap(false);

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

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);
    layout->addWidget(m_filter);
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

    // Preserve expansion/selection across the model's automatic rebuilds.
    connect(m_proxy, &QAbstractItemModel::modelAboutToBeReset,
            this, &DsContentBrowserWidget::captureViewState);
    connect(m_proxy, &QAbstractItemModel::modelReset,
            this, &DsContentBrowserWidget::restoreViewState);
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

    QStringList keys = record.keys();
    std::sort(keys.begin(), keys.end(), [](const QString &a, const QString &b) {
        return a.localeAwareCompare(b) < 0;
    });

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
        }
        m_fields->setItem(row, 0, keyItem);
        m_fields->setItem(row, 1, valItem);
    }

    // Auto-preview the record's first media field, if any.
    if (!firstResourceKey.isEmpty())
        m_preview->show(record.resource(firstResourceKey));
    else
        m_preview->clear();
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

#include "settings/dsSettingsViewerWidget.h"
#include "settings/dsSettings.h"
#include "settings/dsSettingsFile.h"
#include "settings/dsSettingsTreeModel.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QListWidget>
#include <QHideEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QShowEvent>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>

#include <limits>

namespace dsqt {

// ── SettingsValueDelegate ─────────────────────────────────────────────────────
// Column 1 editor. Picks the editor widget from the leaf's schema constraints
// (SettingsTreeModel::ConstraintsRole — a {min, max, step, options} map, any
// subset, sourced from an optional "<name>.schema.toml"):
//   - "options" present            → QComboBox restricted to those choices.
//   - "min" and/or "max" present   → QSpinBox / QDoubleSpinBox (picked by the
//                                    leaf's actual type), bounded accordingly
//                                    and stepped by "step" if given.
//   - no constraints                → QColorDialog for colours (via
//                                    editorEvent), plain QLineEdit otherwise.
// A leaf with no schema entry behaves exactly as before this feature existed.

class SettingsValueDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget *createEditor(QWidget *parent,
                          const QStyleOptionViewItem &,
                          const QModelIndex &index) const override
    {
        const QVariant raw = index.data(Qt::EditRole);
        if (raw.metaType() == QMetaType::fromType<QColor>())
            return nullptr; // handled via editorEvent

        const QVariantMap constraints = index.data(SettingsTreeModel::ConstraintsRole).toMap();

        if (constraints.contains(QStringLiteral("options"))) {
            auto *combo = new QComboBox(parent);
            combo->setEditable(false);
            combo->setAutoFillBackground(true); // opaque — no cell content bleeding through
            for (const QVariant &option : constraints.value(QStringLiteral("options")).toList())
                combo->addItem(SettingsTreeModel::displayString(option));
            // QComboBox doesn't reliably trigger the base delegate's normal
            // focus-out auto-commit at the right moment (or with the freshly
            // picked value) once its popup closes — commit explicitly instead.
            connect(combo, &QComboBox::activated, this, [this, combo] {
                auto *self = const_cast<SettingsValueDelegate *>(this);
                emit self->commitData(combo);
                emit self->closeEditor(combo);
            });
            return combo;
        }

        if (constraints.contains(QStringLiteral("min")) || constraints.contains(QStringLiteral("max"))) {
            if (raw.metaType() == QMetaType::fromType<double>()
                || raw.metaType() == QMetaType::fromType<float>()) {
                auto *spin = new QDoubleSpinBox(parent);
                spin->setAutoFillBackground(true);
                spin->setDecimals(6);
                spin->setRange(constraints.value(QStringLiteral("min"), -1.0e18).toDouble(),
                               constraints.value(QStringLiteral("max"), 1.0e18).toDouble());
                if (constraints.contains(QStringLiteral("step")))
                    spin->setSingleStep(constraints.value(QStringLiteral("step")).toDouble());
                connect(spin, &QAbstractSpinBox::editingFinished, this, [this, spin] {
                    emit const_cast<SettingsValueDelegate *>(this)->commitData(spin);
                });
                return spin;
            }
            if (raw.metaType() == QMetaType::fromType<int>()
                || raw.metaType() == QMetaType::fromType<qlonglong>()) {
                auto *spin = new QSpinBox(parent);
                spin->setAutoFillBackground(true);
                spin->setRange(constraints.value(QStringLiteral("min"), std::numeric_limits<int>::min()).toInt(),
                               constraints.value(QStringLiteral("max"), std::numeric_limits<int>::max()).toInt());
                if (constraints.contains(QStringLiteral("step")))
                    spin->setSingleStep(constraints.value(QStringLiteral("step")).toInt());
                connect(spin, &QAbstractSpinBox::editingFinished, this, [this, spin] {
                    emit const_cast<SettingsValueDelegate *>(this)->commitData(spin);
                });
                return spin;
            }
            // min/max on a non-numeric value doesn't make sense — fall through
            // to the plain line editor below.
        }

        return new QLineEdit(parent);
    }

    // The base implementation sizes some editor widgets (QComboBox in
    // particular) from their own size hint / a style-specific sub-rect
    // rather than the full cell, which can leave a sliver of the original
    // cell content visible around or behind the editor. Force full coverage.
    void updateEditorGeometry(QWidget *editor,
                              const QStyleOptionViewItem &option,
                              const QModelIndex &) const override
    {
        editor->setGeometry(option.rect);
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        if (auto *combo = qobject_cast<QComboBox *>(editor)) {
            combo->setCurrentIndex(combo->findText(index.data(Qt::DisplayRole).toString()));
            return;
        }
        if (auto *spin = qobject_cast<QDoubleSpinBox *>(editor)) {
            spin->setValue(index.data(Qt::EditRole).toDouble());
            return;
        }
        if (auto *spin = qobject_cast<QSpinBox *>(editor)) {
            spin->setValue(static_cast<int>(index.data(Qt::EditRole).toLongLong()));
            return;
        }
        if (auto *le = qobject_cast<QLineEdit *>(editor))
            le->setText(index.data(Qt::DisplayRole).toString());
    }

    void setModelData(QWidget *editor,
                      QAbstractItemModel *model,
                      const QModelIndex &index) const override
    {
        if (auto *combo = qobject_cast<QComboBox *>(editor)) {
            model->setData(index, combo->currentText(), Qt::EditRole);
            return;
        }
        if (auto *spin = qobject_cast<QDoubleSpinBox *>(editor)) {
            model->setData(index, spin->value(), Qt::EditRole);
            return;
        }
        if (auto *spin = qobject_cast<QSpinBox *>(editor)) {
            // Reproduce the leaf's original int/qlonglong metatype so
            // SettingsTreeModel::setData()'s exact-type fast path is used.
            const QVariant raw = index.data(Qt::EditRole);
            if (raw.metaType() == QMetaType::fromType<qlonglong>())
                model->setData(index, static_cast<qlonglong>(spin->value()), Qt::EditRole);
            else
                model->setData(index, spin->value(), Qt::EditRole);
            return;
        }
        if (auto *le = qobject_cast<QLineEdit *>(editor))
            model->setData(index, le->text(), Qt::EditRole);
    }

    bool editorEvent(QEvent *event,
                     QAbstractItemModel *model,
                     const QStyleOptionViewItem &option,
                     const QModelIndex &index) override
    {
        if (event->type() == QEvent::MouseButtonDblClick) {
            const QVariant raw = index.data(Qt::EditRole);
            if (raw.metaType() == QMetaType::fromType<QColor>()) {
                QColor color = QColorDialog::getColor(
                    raw.value<QColor>(),
                    option.widget ? option.widget->window() : nullptr,
                    QObject::tr("Pick colour"),
                    QColorDialog::ShowAlphaChannel);
                if (color.isValid())
                    model->setData(index, QVariant::fromValue(color), Qt::EditRole);
                return true;
            }
        }
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }
};

// ── ArrayEditorDialog ─────────────────────────────────────────────────────────
// Lets the user add, remove and reorder items in a QVariantList setting.
// Items can be dragged to reorder them inside the list view.
// On accept the dialog calls setOverride(path, updatedList) directly.

class ArrayEditorDialog : public QDialog
{
public:
    ArrayEditorDialog(const QString      &path,
                      const QVariantList &items,
                      SettingsFile       *sf,
                      QWidget            *parent = nullptr)
        : QDialog(parent, Qt::Dialog)
        , m_path(path)
        , m_sf(sf)
    {
        setWindowTitle(tr("Edit list: %1").arg(path));
        resize(420, 380);

        // ── List view ──
        m_list = new QListWidget;
        m_list->setDragDropMode(QAbstractItemView::InternalMove);
        m_list->setDefaultDropAction(Qt::MoveAction);
        m_list->setDropIndicatorShown(true);
        for (const QVariant &v : items) {
            auto *li = new QListWidgetItem(SettingsTreeModel::displayString(v));
            li->setData(Qt::UserRole, v);
            li->setFlags(li->flags() | Qt::ItemIsEditable);
            m_list->addItem(li);
        }

        // ── Toolbar row ──
        auto *btnAdd    = new QPushButton(tr("Add"));
        auto *btnRemove = new QPushButton(tr("Remove"));
        auto *btnUp     = new QPushButton(tr("▲"));
        auto *btnDown   = new QPushButton(tr("▼"));
        btnUp->setFixedWidth(32);
        btnDown->setFixedWidth(32);

        auto *toolRow = new QHBoxLayout;
        toolRow->addWidget(btnAdd);
        toolRow->addWidget(btnRemove);
        toolRow->addStretch();
        toolRow->addWidget(btnUp);
        toolRow->addWidget(btnDown);

        // ── Buttons ──
        auto *bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

        auto *vl = new QVBoxLayout(this);
        vl->addWidget(m_list);
        vl->addLayout(toolRow);
        vl->addWidget(bbox);

        connect(btnAdd,    &QPushButton::clicked, this, &ArrayEditorDialog::onAdd);
        connect(btnRemove, &QPushButton::clicked, this, &ArrayEditorDialog::onRemove);
        connect(btnUp,     &QPushButton::clicked, this, &ArrayEditorDialog::onMoveUp);
        connect(btnDown,   &QPushButton::clicked, this, &ArrayEditorDialog::onMoveDown);
        connect(bbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(this, &QDialog::accepted, this, &ArrayEditorDialog::applyChanges);
    }

private:
    void onAdd()
    {
        // Clone the type from the last item; fall back to QString for empty lists.
        QVariant proto;
        if (m_list->count() > 0)
            proto = m_list->item(m_list->count() - 1)->data(Qt::UserRole);

        const QVariant newVal = defaultValue(proto.metaType());
        auto *li = new QListWidgetItem(SettingsTreeModel::displayString(newVal));
        li->setData(Qt::UserRole, newVal);
        li->setFlags(li->flags() | Qt::ItemIsEditable);
        m_list->addItem(li);
        m_list->setCurrentItem(li);
        m_list->editItem(li);
    }

    void onRemove()
    {
        delete m_list->currentItem();
    }

    void onMoveUp()
    {
        const int row = m_list->currentRow();
        if (row <= 0)
            return;
        QListWidgetItem *item = m_list->takeItem(row);
        m_list->insertItem(row - 1, item);
        m_list->setCurrentRow(row - 1);
    }

    void onMoveDown()
    {
        const int row = m_list->currentRow();
        if (row < 0 || row >= m_list->count() - 1)
            return;
        QListWidgetItem *item = m_list->takeItem(row);
        m_list->insertItem(row + 1, item);
        m_list->setCurrentRow(row + 1);
    }

    void applyChanges()
    {
        // Find the first valid MetaType to guide type-aware parsing of edited text.
        QMetaType mt;
        for (int i = 0; i < m_list->count(); ++i) {
            const QVariant orig = m_list->item(i)->data(Qt::UserRole);
            if (orig.isValid()) { mt = orig.metaType(); break; }
        }

        QVariantList list;
        list.reserve(m_list->count());
        for (int i = 0; i < m_list->count(); ++i) {
            QListWidgetItem *li   = m_list->item(i);
            const QVariant   orig = li->data(Qt::UserRole);
            const QString    text = li->text();
            // If the display text hasn't changed, preserve the original QVariant
            // (keeps exact type, avoids unnecessary re-parsing).
            if (orig.isValid() && text == SettingsTreeModel::displayString(orig))
                list.append(orig);
            else
                list.append(parseAs(text, orig.isValid() ? orig.metaType() : mt));
        }

        m_sf->setOverride(m_path, QVariant::fromValue(list));
    }

    // Returns a sensible zero/empty default for the given type.
    static QVariant defaultValue(const QMetaType mt)
    {
        if (mt == QMetaType::fromType<bool>())      return false;
        if (mt == QMetaType::fromType<int>())       return 0;
        if (mt == QMetaType::fromType<qlonglong>()) return qlonglong{0};
        if (mt == QMetaType::fromType<double>())    return 0.0;
        if (mt == QMetaType::fromType<float>())     return 0.0f;
        return QString{};
    }

    // Parses a display string back to the target MetaType.
    static QVariant parseAs(const QString &text, const QMetaType mt)
    {
        if (mt == QMetaType::fromType<bool>())
            return text == QLatin1String("true") || text == QLatin1String("1");
        if (mt == QMetaType::fromType<int>()) {
            bool ok; const int v = text.toInt(&ok);
            if (ok) return v;
        } else if (mt == QMetaType::fromType<qlonglong>()) {
            bool ok; const qlonglong v = text.toLongLong(&ok);
            if (ok) return v;
        } else if (mt == QMetaType::fromType<double>() || mt == QMetaType::fromType<float>()) {
            bool ok; const double v = text.toDouble(&ok);
            if (ok) return v;
        }
        return text;
    }

    QString      m_path;
    SettingsFile *m_sf;
    QListWidget  *m_list;
};

// ── SettingsFilterProxyModel ──────────────────────────────────────────────────
// Filters the tree by a plain case-insensitive substring match against a row's
// key (column 0), display value (column 1), or full dotted path (FullPathRole)
// — the same fields the legacy viewer's search matched against. A row stays
// visible if it matches directly or if any descendant matches, so the path
// down to a match is never hidden.

class SettingsFilterProxyModel : public QSortFilterProxyModel
{
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    void setFilterKeyword(const QString &text)
    {
        if (m_keyword == text)
            return;
        m_keyword = text;
        invalidateFilter(); // re-run filterAcceptsRow() for every row
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

        // Keep this row if any descendant matches.
        for (int i = 0; i < model->rowCount(index); ++i) {
            if (filterAcceptsRow(i, index))
                return true;
        }
        return false;
    }

private:
    bool rowMatches(const QModelIndex &index) const
    {
        const QString key      = index.data(Qt::DisplayRole).toString();
        const QString value    = index.siblingAtColumn(1).data(Qt::DisplayRole).toString();
        const QString fullPath = index.data(SettingsTreeModel::FullPathRole).toString();

        return key.contains(m_keyword, Qt::CaseInsensitive)
            || value.contains(m_keyword, Qt::CaseInsensitive)
            || fullPath.contains(m_keyword, Qt::CaseInsensitive);
    }

    QString m_keyword;
};

// Recursively opens a persistent editor (column 1) for every visible leaf
// that carries schema constraints (options/min/max), so a combo box or spin
// box is shown immediately instead of only appearing after a double-click.
// Safe to call repeatedly — indexWidget() is checked first so an already-open
// editor is never duplicated.
static void openConstraintEditors(QTreeView *view, const QModelIndex &parent = {})
{
    QAbstractItemModel *model = view->model();
    const int rows = model->rowCount(parent);
    for (int row = 0; row < rows; ++row) {
        const QModelIndex idx0 = model->index(row, 0, parent);
        if (idx0.data(SettingsTreeModel::IsLeafRole).toBool()) {
            const QVariantMap constraints = idx0.data(SettingsTreeModel::ConstraintsRole).toMap();
            if (constraints.contains(QStringLiteral("options"))
                || constraints.contains(QStringLiteral("min"))
                || constraints.contains(QStringLiteral("max"))) {
                const QModelIndex idx1 = model->index(row, 1, parent);
                if (!view->indexWidget(idx1))
                    view->openPersistentEditor(idx1);
            }
        } else {
            openConstraintEditors(view, idx0);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────

SettingsViewerWidget::SettingsViewerWidget(Settings *settings, QWidget *parent)
    : QWidget(parent, Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint
                          | Qt::WindowCloseButtonHint | Qt::WindowMaximizeButtonHint)
    , m_settings(settings)
    , m_tabs(new QTabWidget(this))
{
    setWindowTitle("Settings");
    resize(1000, 600);

    auto *saveBtn    = new QPushButton(tr("Save…"));   // "Save…"
    auto *restoreBtn = new QPushButton(tr("Restore…")); // "Restore…"

    auto *btnRow = new QHBoxLayout;
    btnRow->setContentsMargins(6, 4, 6, 6);
    btnRow->addStretch();
    btnRow->addWidget(saveBtn);
    btnRow->addWidget(restoreBtn);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_tabs);
    layout->addLayout(btnRow);

    connect(settings, &Settings::instancesChanged, this, &SettingsViewerWidget::rebuild);
    connect(saveBtn,    &QPushButton::clicked, this, &SettingsViewerWidget::onSave);
    connect(restoreBtn, &QPushButton::clicked, this, &SettingsViewerWidget::onRestore);
    rebuild();
}

void SettingsViewerWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    emit visibilityChanged(true);
}

void SettingsViewerWidget::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    emit visibilityChanged(false);
}

void SettingsViewerWidget::rebuild()
{
    // Remember the active tab name so we can restore it after the rebuild.
    const QString current = m_tabs->count() > 0
                                ? m_tabs->tabText(m_tabs->currentIndex())
                                : QString{};

    // clear() reparents the tab widgets to nullptr and schedules deletion.
    m_tabs->clear();

    for (const QString &name : m_settings->settingsNames()) {
        auto *view  = new QTreeView;
        auto *model = new SettingsTreeModel(view);
        auto *proxy = new SettingsFilterProxyModel(view);

        model->setSettingsFile(m_settings->settingsFile(name));

        // Optional "<name>.schema.toml" providing min/max/step/options metadata
        // for the value editor below. Given a manager but never a registered
        // name, so it gets live-reload/search-path resolution for free without
        // becoming its own tab or being reachable via Settings.find()/QML.
        // If the file doesn't exist, every constraint lookup is simply empty —
        // editing behaves exactly as it would with no schema at all.
        auto *schema = new SettingsFile(view);
        schema->setManager(m_settings);
        schema->setFileName(name + QStringLiteral(".schema.toml"));
        model->setSchema(schema);

        proxy->setSourceModel(model);
        view->setModel(proxy);
        view->setItemDelegateForColumn(1, new SettingsValueDelegate(view));
        view->setAlternatingRowColors(true);
        view->setUniformRowHeights(true);
        view->setRootIsDecorated(true);
        view->setContextMenuPolicy(Qt::CustomContextMenu);

        // Persistent editors (combo/spin boxes) are real focus-accepting
        // widgets, so clicking on a different row doesn't automatically hand
        // keyboard focus back — nothing else in the row has a widget to take
        // it. Reclaim focus for the view itself whenever the current index
        // moves somewhere that isn't its own persistent editor.
        connect(view->selectionModel(), &QItemSelectionModel::currentChanged, view,
                [view](const QModelIndex &current, const QModelIndex &) {
                    if (current.isValid() && !view->indexWidget(current))
                        view->setFocus();
                });

        // Expand after every model reset (file reload, etc.), and re-open
        // persistent editors for schema-constrained leaves — the reset tears
        // down any that were open, since it invalidates every old index.
        connect(model, &SettingsTreeModel::modelReset, view, [view] {
            view->expandAll();
            openConstraintEditors(view);
        });
        view->expandAll();
        openConstraintEditors(view);

        auto *searchBox = new QLineEdit;
        searchBox->setPlaceholderText(tr("Filter by key, path, or value…"));
        searchBox->setClearButtonEnabled(true);
        connect(searchBox, &QLineEdit::textChanged, view, [proxy, view](const QString &text) {
            proxy->setFilterKeyword(text);
            view->expandAll(); // reveal matches even under previously-collapsed branches
            openConstraintEditors(view); // re-open any that filtering just brought back into view
        });

        // Open the array editor when the user double-clicks a list node.
        connect(view, &QTreeView::doubleClicked, view,
                [model, view](const QModelIndex &idx) {
                    if (!idx.isValid())
                        return;
                    if (idx.data(SettingsTreeModel::IsLeafRole).toBool())
                        return;
                    if (!idx.data(SettingsTreeModel::IsListRole).toBool())
                        return;
                    const QString path = idx.data(SettingsTreeModel::FullPathRole).toString();
                    if (path.isEmpty() || !model->settingsFile())
                        return;
                    const QVariantList items = model->settingsFile()->value(path).toList();
                    ArrayEditorDialog dlg(path, items, model->settingsFile(), view->window());
                    dlg.exec();
                    // applyChanges() inside the dialog calls setOverride, which
                    // triggers settingsRebuilt → SettingsTreeModel::rebuild()
                    // automatically — nothing more needed here.
                });

        // Column widths — interactive so the user can drag them.
        connect(view, &QTreeView::customContextMenuRequested, view,
                [model, view](const QPoint &pos) {
                    const QModelIndex idx = view->indexAt(pos);
                    if (!idx.isValid() || !model->settingsFile())
                        return;

                    const QString path = idx.data(SettingsTreeModel::FullPathRole).toString();
                    if (path.isEmpty())
                        return;

                    const QString provenance = idx.data(SettingsTreeModel::ProvenanceRole).toString();
                    QMenu menu(view);

                    if (provenance == QStringLiteral("override")) {
                        menu.addAction(QObject::tr("Revert"), view, [model, path] {
                            if (auto *settingsFile = model->settingsFile())
                                settingsFile->resetOverride(path);
                        });
                    } else if (!provenance.isEmpty() && provenance != QStringLiteral("default")
                               && QFileInfo::exists(provenance)) {
                        menu.addAction(QObject::tr("Open File"), view, [view, provenance] {
                            if (!QDesktopServices::openUrl(QUrl::fromLocalFile(provenance))) {
                                QMessageBox::warning(view->window(),
                                                     QObject::tr("Open File"),
                                                     QObject::tr("Could not open %1.").arg(provenance));
                            }
                        });
                    }

                    if (!menu.isEmpty())
                        menu.exec(view->viewport()->mapToGlobal(pos));
                });

        QHeaderView *header = view->header();
        header->setSectionResizeMode(QHeaderView::Interactive);
        header->resizeSection(0, 260);
        header->resizeSection(1, 380);
        // Type column fills the remainder automatically via Stretch on the last section.
        header->setStretchLastSection(true);

        // Render snake_case settings file names as "Title Case" tab labels,
        // e.g. "app_settings" -> "App Settings".
        QString label = name;
        label.replace(QLatin1Char('_'), QLatin1Char(' '));
        bool atWordStart = true;
        for (QChar &ch : label) {
            if (ch.isSpace()) {
                atWordStart = true;
            } else if (atWordStart) {
                ch = ch.toUpper();
                atWordStart = false;
            }
        }
        auto *page = new QWidget;
        auto *pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(4, 4, 4, 4);
        pageLayout->setSpacing(4);
        pageLayout->addWidget(searchBox);
        pageLayout->addWidget(view);

        m_tabs->addTab(page, label);
    }

    // Restore the previously active tab.
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (m_tabs->tabText(i) == current) {
            m_tabs->setCurrentIndex(i);
            break;
        }
    }
}

SettingsFile *SettingsViewerWidget::currentSettingsFile() const
{
    const int idx = m_tabs->currentIndex();
    if (idx < 0)
        return nullptr;
    QWidget *page = m_tabs->widget(idx);
    if (!page)
        return nullptr;
    auto *view = page->findChild<QTreeView *>();
    if (!view)
        return nullptr;
    // view->model() is now the SettingsFilterProxyModel; unwrap it to get
    // at the underlying SettingsTreeModel.
    auto *proxy = qobject_cast<QSortFilterProxyModel *>(view->model());
    auto *model = qobject_cast<SettingsTreeModel *>(proxy ? proxy->sourceModel() : view->model());
    if (!model)
        return nullptr;
    return model->settingsFile();
}

void SettingsViewerWidget::onSave()
{
    auto *sf = currentSettingsFile();
    if (!sf)
        return;

    if (sf->overrides().isEmpty()) {
        QMessageBox::information(this, tr("Save settings"),
                                 tr("There are no overrides to save."));
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        this,
        tr("Save overrides"),
        {},
        tr("TOML files (*.toml);;All files (*)"));
    if (path.isEmpty())
        return;

    const QString err = sf->saveOverridesTo(path);
    if (!err.isEmpty())
        QMessageBox::warning(this, tr("Save settings"), err);
}

void SettingsViewerWidget::onRestore()
{
    auto *sf = currentSettingsFile();
    if (!sf)
        return;

    if (sf->overrides().isEmpty()) {
        QMessageBox::information(this, tr("Restore settings"),
                                 tr("There are no overrides to restore."));
        return;
    }

    const auto answer = QMessageBox::question(
        this,
        tr("Restore settings"),
        tr("Reset all overrides for this settings file?\n"
           "Values will revert to their file or default values."),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (answer == QMessageBox::Yes)
        sf->resetOverrides();
}

}

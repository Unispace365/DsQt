#include "settings/dsSettingsViewerWidget.h"
#include "settings/dsSettings.h"
#include "settings/dsSettingsFile.h"
#include "settings/dsSettingsTreeModel.h"

#include <QApplication>
#include <QColor>
#include <QColorDialog>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QListWidget>
#include <QHideEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QShowEvent>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>

namespace dsqt {

// ── SettingsValueDelegate ─────────────────────────────────────────────────────
// Column 1 editor: QColorDialog for colours, QLineEdit for everything else.

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
        return new QLineEdit(parent);
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        if (auto *le = qobject_cast<QLineEdit *>(editor))
            le->setText(index.data(Qt::DisplayRole).toString());
    }

    void setModelData(QWidget *editor,
                      QAbstractItemModel *model,
                      const QModelIndex &index) const override
    {
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

        model->setSettingsFile(m_settings->settingsFile(name));
        view->setModel(model);
        view->setItemDelegateForColumn(1, new SettingsValueDelegate(view));
        view->setAlternatingRowColors(true);
        view->setUniformRowHeights(true);
        view->setRootIsDecorated(true);
        view->setContextMenuPolicy(Qt::CustomContextMenu);

        // Expand after every model reset (file reload, etc.).
        connect(model, &SettingsTreeModel::modelReset, view, &QTreeView::expandAll);
        view->expandAll();

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
        m_tabs->addTab(view, label);
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
    auto *view  = qobject_cast<QTreeView *>(m_tabs->widget(idx));
    if (!view)
        return nullptr;
    auto *model = qobject_cast<SettingsTreeModel *>(view->model());
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

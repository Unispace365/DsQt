#include "settings/dsSettingsViewerWidget.h"
#include "settings/dsSettings.h"
#include "settings/dsSettingsFile.h"
#include "settings/dsSettingsTreeModel.h"
#include "settings/dsSettingsUtils.h"
#include "settings/dsTomlEditor.h"
#include "core/dsEnvironment.h"

#include <QApplication>
#include <QFontDatabase>
#include <QCheckBox>
#include <QResizeEvent>
#include <QFrame>
#include <QToolButton>
#include <QMutex>
#include <QPalette>
#include <QCoreApplication>
#include <QStyle>
#include <QThreadPool>
#include <QPointer>
#include <QHash>
#include <QDir>
#include <QRegularExpression>
#include <QRadioButton>
#include <QLabel>
#include <QGridLayout>
#include <QButtonGroup>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <algorithm>
#include <functional>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QListWidget>
#include <QHideEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QShowEvent>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTimer>
#include <QToolTip>
#include <QTreeView>
#include <QUrl>
#include <QVariantAnimation>
#include <QVBoxLayout>

namespace dsqt {

// ── OptionsComboBox ───────────────────────────────────────────────────────────
// The editor for settings whose schema lists options. It opens its popup as
// soon as it is shown and reports when the popup closes, so the delegate can
// close the editor whether or not an option was picked.

class OptionsComboBox : public QComboBox
{
public:
    using QComboBox::QComboBox;

    std::function<void()> onPopupHidden;

    void hidePopup() override
    {
        QComboBox::hidePopup();
        if (onPopupHidden)
            onPopupHidden();
    }
};

// ── SettingsValueDelegate ─────────────────────────────────────────────────────
// Column 1 (values):
//  - colours: QColorDialog on double-click;
//  - schema sliders: painted in the cell and dragged directly, no editor widget;
//  - schema options: painted as a flat dropdown, opens an OptionsComboBox;
//  - file and folder paths (schema editor, or detected): painted browse button
//    that opens a file or folder dialog; the result is stored with a path
//    variable (%LOCAL%, %APP%, ...) where one applies;
//  - Booleans: painted on/off switch, flipped with a click or Space;
//  - everything else (and a slider's exact value, via double-click on its
//    text or F2): QLineEdit.
//
// Sliders and dropdowns are painted rather than being persistent widgets, so
// every row keeps exactly the same height and look as a plain text row, and
// nothing has to be recreated when the model changes.

class SettingsValueDelegate : public QStyledItemDelegate
{
public:
    explicit SettingsValueDelegate(QAbstractItemView *view)
        : QStyledItemDelegate(view)
        , m_view(view)
    {
        // The delegate drives slider drags itself: an item view only forwards
        // presses to its delegate, never the moves that follow.
        view->viewport()->installEventFilter(this);
        view->viewport()->setMouseTracking(true); // cursor feedback
        view->viewport()->setAttribute(Qt::WA_Hover);
        view->installEventFilter(this); // Space flips a switch

        // Slides a switch's knob over when it is flipped.
        m_switchAnimation = new QVariantAnimation(this);
        m_switchAnimation->setDuration(120);
        m_switchAnimation->setEasingCurve(QEasingCurve::OutCubic);
        QObject::connect(m_switchAnimation, &QVariantAnimation::valueChanged, this, [this] {
            if (m_switchIndex.isValid())
                m_view->viewport()->update(m_view->visualRect(m_switchIndex));
        });
    }

    // ── Painting ──

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        // One height for every row, with room for a slider handle and a
        // dropdown frame. The tree uses uniform row heights, so this applies
        // to all rows, whatever they contain.
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setHeight(std::max(size.height(), rowHeight(option.fontMetrics)));
        return size;
    }

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        // Under an open editor only the background is painted: some styles draw
        // a frameless combo box without filling it, so the cell's own text
        // would show through.
        if (m_editing.isValid() && index == m_editing) {
            QStyleOptionViewItem opt(option);
            initStyleOption(&opt, index);
            opt.text.clear();
            opt.icon = QIcon();
            const QWidget *widget = opt.widget;
            QStyle *style = widget ? widget->style() : QApplication::style();
            style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
            return;
        }

        const SettingsSchemaEntry entry = schemaEntry(index);
        if (entry.kind == SettingsSchemaEntry::Kind::None) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        const QString text = opt.text;
        const QWidget *widget = opt.widget;
        QStyle *style = widget ? widget->style() : QApplication::style();

        // Background, selection and focus exactly as for any other row.
        opt.text.clear();
        opt.icon = QIcon();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

        const Colors colors = colorsFor(opt);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setFont(opt.font);

        switch (entry.kind) {
        case SettingsSchemaEntry::Kind::Slider:
            paintSlider(painter, opt, index, entry, text, colors);
            break;
        case SettingsSchemaEntry::Kind::Options:
            paintOptions(painter, opt, index, entry, text, colors);
            break;
        case SettingsSchemaEntry::Kind::File:
        case SettingsSchemaEntry::Kind::Folder:
            paintPath(painter, opt, entry, text, colors);
            break;
        case SettingsSchemaEntry::Kind::Switch:
            paintSwitch(painter, opt, index, text, colors);
            break;
        case SettingsSchemaEntry::Kind::None:
            break;
        }

        painter->restore();
    }

    // ── Editing ──

    QWidget *createEditor(QWidget *parent,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override
    {
        const QVariant raw = index.data(Qt::EditRole);
        if (raw.metaType() == QMetaType::fromType<QColor>())
            return nullptr; // handled via editorEvent

        const SettingsSchemaEntry entry = schemaEntry(index);
        if (entry.kind == SettingsSchemaEntry::Kind::Options) {
            auto *combo = new OptionsComboBox(parent);
            combo->setFrame(false);
            combo->setFont(option.font);
            for (qsizetype i = 0; i < entry.options.size(); ++i)
                combo->addItem(entry.labels.at(i), entry.options.at(i));

            auto *self = const_cast<SettingsValueDelegate *>(this);
            QObject::connect(combo, &QComboBox::activated, combo, [self, combo] {
                emit self->commitData(combo);
            });
            // QComboBox hides its popup before it emits activated, so close
            // the editor only once the event loop is back, after the commit.
            combo->onPopupHidden = [self, combo] {
                QMetaObject::invokeMethod(combo, [self, combo] {
                    emit self->closeEditor(combo);
                }, Qt::QueuedConnection);
            };
            QTimer::singleShot(0, combo, &QComboBox::showPopup);
            return trackEditor(combo, index);
        }

        return trackEditor(new QLineEdit(parent), index);
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        if (auto *combo = qobject_cast<QComboBox *>(editor))
            combo->setCurrentIndex(schemaEntry(index).optionIndex(index.data(Qt::EditRole)));
        else if (auto *le = qobject_cast<QLineEdit *>(editor))
            le->setText(index.data(Qt::DisplayRole).toString());
    }

    void setModelData(QWidget *editor,
                      QAbstractItemModel *model,
                      const QModelIndex &index) const override
    {
        if (auto *combo = qobject_cast<QComboBox *>(editor)) {
            if (combo->currentIndex() >= 0)
                model->setData(index, combo->currentData(), Qt::EditRole);
            return;
        }

        auto *le = qobject_cast<QLineEdit *>(editor);
        if (!le)
            return;
        if (!model->setData(index, le->text(), Qt::EditRole)) {
            // The model rejects text that does not fit the setting's type; say
            // so instead of letting the old value silently reappear.
            const QString type = index.siblingAtColumn(2).data(Qt::DisplayRole).toString();
            QToolTip::showText(le->mapToGlobal(QPoint(0, le->height())),
                               QObject::tr("\"%1\" is not a valid %2.").arg(le->text(), type),
                               le);
        }
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

protected:
    // Mouse handling for painted sliders and dropdowns, on the view's viewport.
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == m_view
            && (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride)) {
            // Space flips the current row's switch, whichever column is current.
            auto *ke = static_cast<QKeyEvent *>(event);
            const QModelIndex value = m_view->currentIndex().siblingAtColumn(1);
            if (ke->key() == Qt::Key_Space && ke->modifiers() == Qt::NoModifier
                && !m_editing.isValid()
                && schemaEntry(value).kind == SettingsSchemaEntry::Kind::Switch) {
                // Accepting the override claims Space from application-wide
                // shortcuts (an app's own "SPACE" Shortcut); the key press follows.
                if (event->type() == QEvent::ShortcutOverride) {
                    ke->accept();
                    return true;
                }
                toggle(value);
                return true;
            }
        }
        if (watched != m_view->viewport())
            return QStyledItemDelegate::eventFilter(watched, event);

        switch (event->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonDblClick: {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() != Qt::LeftButton)
                break;
            const QPoint pos = me->position().toPoint();
            const QModelIndex index = m_view->indexAt(pos);
            switch (hitTest(index, pos)) {
            case Hit::Slider:
                m_view->setCurrentIndex(index);
                m_drag = index;
                dragTo(pos);
                return true;
            case Hit::Options:
                m_view->setCurrentIndex(index);
                m_view->edit(index);
                return true;
            case Hit::Browse:
                m_view->setCurrentIndex(index);
                if (event->type() == QEvent::MouseButtonPress) {
                    // Open the dialog once the press has been handled, not
                    // from inside it.
                    const QPersistentModelIndex target(index);
                    QTimer::singleShot(0, m_view, [this, target] { browse(target); });
                }
                return true; // a double-click on the button must not start text editing
            case Hit::Switch:
                // A double-click arrives instead of a second press: it flips
                // the switch back, as two clicks would.
                m_view->setCurrentIndex(index);
                toggle(index);
                return true;
            case Hit::None:
                break;
            }
            break;
        }
        case QEvent::MouseMove: {
            auto *me = static_cast<QMouseEvent *>(event);
            const QPoint pos = me->position().toPoint();
            if (m_drag.isValid()) {
                if (me->buttons() & Qt::LeftButton) {
                    dragTo(pos);
                    return true;
                }
                endDrag(); // the release happened outside the window
            }
            if (hitTest(m_view->indexAt(pos), pos) != Hit::None)
                m_view->viewport()->setCursor(Qt::PointingHandCursor);
            else
                m_view->viewport()->unsetCursor();
            break;
        }
        case QEvent::MouseButtonRelease:
            if (m_drag.isValid() && static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
                endDrag();
                return true;
            }
            break;
        case QEvent::Leave:
            if (!m_drag.isValid())
                m_view->viewport()->unsetCursor();
            break;
        default:
            break;
        }
        return QStyledItemDelegate::eventFilter(watched, event);
    }

private:
    enum class Hit { None, Slider, Options, Browse, Switch };

    struct Colors
    {
        QColor text;   // value text, chevron
        QColor groove; // unfilled part of a slider, dropdown frame
        QColor fill;   // filled part of a slider, handle centre
        QColor handle; // handle body
    };

    // Positions of a slider's parts inside a cell. The value text gets a fixed
    // width (the widest of min and max, in bold so an override does not shift
    // anything), so the track stays put while the value changes.
    struct SliderLayout
    {
        QRect  text;
        QRectF track; // the groove line; empty when the cell is too narrow
        qreal  handleRadius = 0;
    };

    static int rowHeight(const QFontMetrics &fm) { return fm.height() + 8; }

    static SettingsSchemaEntry schemaEntry(const QModelIndex &index)
    {
        if (index.column() != 1)
            return {};
        return index.data(SettingsTreeModel::SchemaEntryRole).value<SettingsSchemaEntry>();
    }

    int textMargin() const
    {
        // The horizontal padding QStyledItemDelegate uses for item text.
        return m_view->style()->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, m_view) + 1;
    }

    SliderLayout sliderLayout(const QRect &cell,
                              const QFont &font,
                              const SettingsSchemaEntry &entry) const
    {
        QFont bold(font);
        bold.setBold(true);
        const QFontMetrics fm(bold);
        const int valueWidth = std::max({fm.horizontalAdvance(entry.format(entry.min)),
                                         fm.horizontalAdvance(entry.format(entry.max)),
                                         fm.horizontalAdvance(QStringLiteral("0000"))});

        SliderLayout layout;
        const int margin = textMargin();
        layout.text = QRect(cell.left() + margin, cell.top(), valueWidth, cell.height());
        layout.handleRadius = std::clamp(cell.height() / 2.0 - 4.0, 4.0, 8.0);

        const qreal left  = layout.text.right() + 12 + layout.handleRadius;
        const qreal right = cell.right() - 8 - layout.handleRadius;
        if (right - left >= 24) {
            const qreal groove = 4.0;
            layout.track = QRectF(left, cell.center().y() + 0.5 - groove / 2, right - left, groove);
        }
        return layout;
    }

    Hit hitTest(const QModelIndex &index, const QPoint &pos) const
    {
        const SettingsSchemaEntry entry = schemaEntry(index);
        const QRect cell = m_view->visualRect(index);
        if (entry.kind == SettingsSchemaEntry::Kind::Options)
            return cell.contains(pos) ? Hit::Options : Hit::None;
        if (entry.kind == SettingsSchemaEntry::Kind::File
            || entry.kind == SettingsSchemaEntry::Kind::Folder)
            return browseButtonRect(cell).contains(pos) ? Hit::Browse : Hit::None;
        if (entry.kind == SettingsSchemaEntry::Kind::Switch) {
            // The full cell height, a little wider than the switch; the text
            // next to it stays editable with a double-click.
            const QRectF track = switchRect(cell);
            const QRectF area(track.left() - 4, cell.top(), track.width() + 8, cell.height());
            return area.contains(pos) ? Hit::Switch : Hit::None;
        }
        if (entry.kind == SettingsSchemaEntry::Kind::Slider) {
            const SliderLayout layout = sliderLayout(cell, m_view->font(), entry);
            if (layout.track.isEmpty())
                return Hit::None;
            // The whole cell height, from the left of the handle at min to the
            // right of the handle at max; the value text stays editable.
            const qreal r = layout.handleRadius;
            const QRectF area(layout.track.left() - r, cell.top(),
                              layout.track.width() + 2 * r, cell.height());
            return area.contains(pos) ? Hit::Slider : Hit::None;
        }
        return Hit::None;
    }

    void dragTo(const QPoint &pos)
    {
        const QModelIndex index = m_drag;
        const SettingsSchemaEntry entry = schemaEntry(index);
        if (entry.kind != SettingsSchemaEntry::Kind::Slider) {
            endDrag(); // the model changed shape under the drag
            return;
        }
        const SliderLayout layout = sliderLayout(m_view->visualRect(index), m_view->font(), entry);
        if (layout.track.isEmpty())
            return;

        const double t = std::clamp((pos.x() - layout.track.left()) / layout.track.width(), 0.0, 1.0);
        const QVariant value = entry.snapped(entry.min + t * (entry.max - entry.min));
        if (value != index.data(Qt::EditRole))
            m_view->model()->setData(index, value, Qt::EditRole); // applied live
        m_view->viewport()->update(m_view->visualRect(index));
    }

    void endDrag()
    {
        const QModelIndex index = m_drag;
        m_drag = QPersistentModelIndex();
        if (index.isValid())
            m_view->viewport()->update(m_view->visualRect(index));
    }

    static Colors colorsFor(const QStyleOptionViewItem &opt)
    {
        const QPalette::ColorGroup group = !(opt.state & QStyle::State_Enabled) ? QPalette::Disabled
                                           : (opt.state & QStyle::State_Active) ? QPalette::Normal
                                                                                : QPalette::Inactive;
        const bool selected = opt.state & QStyle::State_Selected;

        Colors c;
        c.text = opt.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text);
        c.groove = c.text;
        c.groove.setAlphaF(0.22f);
        // On a selected row the selection is often a shade of the accent (the
        // Windows 11 style paints it a lighter blue), which would swallow an
        // accent fill; use the selection's text colour there instead.
        c.fill = selected ? c.text : opt.palette.color(group, QPalette::Accent);
        c.handle = opt.palette.color(group, QPalette::Base);
        return c;
    }

    void paintSlider(QPainter *painter,
                     const QStyleOptionViewItem &opt,
                     const QModelIndex &index,
                     const SettingsSchemaEntry &entry,
                     const QString &text,
                     const Colors &colors) const
    {
        const SliderLayout layout = sliderLayout(opt.rect, opt.font, entry);

        painter->setPen(colors.text);
        painter->drawText(layout.text, Qt::AlignLeft | Qt::AlignVCenter,
                          opt.fontMetrics.elidedText(text, Qt::ElideRight, layout.text.width()));
        if (layout.track.isEmpty())
            return;

        const QRectF &track = layout.track;
        const qreal radius = track.height() / 2;
        const qreal x = track.left() + entry.ratio(index.data(Qt::EditRole).toDouble()) * track.width();

        painter->setPen(Qt::NoPen);
        painter->setBrush(colors.groove);
        painter->drawRoundedRect(track, radius, radius);
        painter->setBrush(colors.fill);
        painter->drawRoundedRect(QRectF(track.left(), track.top(), x - track.left(), track.height()),
                                 radius, radius);

        // Handle: a ring in the base colour with an accent centre that grows
        // while the row is hovered or the handle is being dragged.
        const QPointF centre(x, track.center().y());
        const qreal r = layout.handleRadius;
        QColor border = colors.text;
        border.setAlphaF(0.25f);
        painter->setPen(QPen(border, 1.0));
        painter->setBrush(colors.handle);
        painter->drawEllipse(centre, r, r);

        const bool active = (opt.state & QStyle::State_MouseOver) || m_drag == index;
        const qreal inner = r * (active ? 0.62 : 0.5);
        painter->setPen(Qt::NoPen);
        painter->setBrush(colors.fill);
        painter->drawEllipse(centre, inner, inner);
    }

    void paintOptions(QPainter *painter,
                      const QStyleOptionViewItem &opt,
                      const QModelIndex &index,
                      const SettingsSchemaEntry &entry,
                      const QString &text,
                      const Colors &colors) const
    {
        const int margin = textMargin();
        const int chevron = 8; // width of the down-arrow
        const QRect cell = opt.rect;

        // Subtle frame on hover so the cell reads as clickable.
        if (opt.state & QStyle::State_MouseOver) {
            painter->setPen(QPen(colors.groove, 1.0));
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(QRectF(cell).adjusted(1.5, 2.5, -1.5, -2.5), 4, 4);
        }

        // A value that is not one of the options is shown in italics.
        const bool known = entry.optionIndex(index.data(Qt::EditRole)) >= 0;
        QFont font(opt.font);
        font.setItalic(!known);
        painter->setFont(font);
        painter->setPen(colors.text);
        const QRect textRect = cell.adjusted(margin, 0, -(margin + chevron + 8), 0);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                          QFontMetrics(font).elidedText(text, Qt::ElideRight, textRect.width()));

        const QPointF tip(cell.right() - margin - 4 - chevron / 2.0, cell.center().y() + 2.5);
        QPainterPath arrow;
        arrow.moveTo(tip + QPointF(-chevron / 2.0, -4));
        arrow.lineTo(tip);
        arrow.lineTo(tip + QPointF(chevron / 2.0, -4));
        painter->setPen(QPen(colors.text, 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(arrow);
    }

    // The switch's track, at the left of the cell, as tall as a slider handle.
    QRectF switchRect(const QRect &cell) const
    {
        const qreal height = std::clamp(cell.height() - 8.0, 12.0, 18.0);
        const qreal width  = std::round(height * 1.8);
        return QRectF(cell.left() + textMargin() + 0.5,
                      cell.top() + (cell.height() - height) / 2.0 + 0.5, width, height);
    }

    // Flips a Boolean and slides the knob over to its new side.
    void toggle(const QModelIndex &index)
    {
        const bool on = index.data(Qt::EditRole).toBool();
        if (!m_view->model()->setData(index, !on, Qt::EditRole))
            return;
        m_switchAnimation->stop();
        m_switchIndex = index;
        m_switchAnimation->setStartValue(on ? 1.0 : 0.0);
        m_switchAnimation->setEndValue(on ? 0.0 : 1.0);
        m_switchAnimation->start();
    }

    static QColor blend(const QColor &a, const QColor &b, qreal t)
    {
        const auto mix = [t](float x, float y) { return x + (y - x) * float(t); };
        return QColor::fromRgbF(mix(a.redF(), b.redF()), mix(a.greenF(), b.greenF()),
                                mix(a.blueF(), b.blueF()), mix(a.alphaF(), b.alphaF()));
    }

    void paintSwitch(QPainter *painter,
                     const QStyleOptionViewItem &opt,
                     const QModelIndex &index,
                     const QString &text,
                     const Colors &colors) const
    {
        const QRectF track = switchRect(opt.rect);
        const bool on = index.data(Qt::EditRole).toBool();

        // 0 = off, 1 = on; in between while the knob slides over.
        qreal t = on ? 1.0 : 0.0;
        if (m_switchAnimation->state() == QAbstractAnimation::Running && index == m_switchIndex)
            t = m_switchAnimation->currentValue().toReal();

        // Track: the groove colour with an outline when off, the accent when on.
        const qreal radius = track.height() / 2;
        QColor outline = colors.text;
        outline.setAlphaF(0.35f * float(1.0 - t));
        painter->setPen(t < 1.0 ? QPen(outline, 1.0) : QPen(Qt::NoPen));
        painter->setBrush(blend(colors.groove, colors.fill, t));
        painter->drawRoundedRect(track, radius, radius);

        // Knob: in the base colour, a little larger while the row is hovered.
        const bool hover = opt.state & QStyle::State_MouseOver;
        const qreal knob = radius - (hover ? 2.0 : 2.5);
        const qreal from = track.left() + radius;
        const qreal to   = track.right() - radius;
        const QPointF centre(from + (to - from) * t, track.center().y());
        QColor border = colors.text;
        border.setAlphaF(0.25f * float(1.0 - t));
        painter->setPen(t < 1.0 ? QPen(border, 1.0) : QPen(Qt::NoPen));
        painter->setBrush(colors.handle);
        painter->drawEllipse(centre, knob, knob);

        // The value as written in the file, slightly dimmed.
        QColor textColor = colors.text;
        textColor.setAlphaF(0.75f);
        const int textLeft = int(std::ceil(track.right())) + 8;
        const QRect textRect(textLeft, opt.rect.top(), opt.rect.right() - textLeft, opt.rect.height());
        painter->setPen(textColor);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                          opt.fontMetrics.elidedText(text, Qt::ElideRight, textRect.width()));
    }

    // The "…" button at the right end of a path cell.
    static QRect browseButtonRect(const QRect &cell)
    {
        const int height = cell.height() - 6;
        const int width  = cell.height() + 4;
        return QRect(cell.right() - 3 - width + 1, cell.top() + 3, width, height);
    }

    // Opens a file or folder dialog for a path cell and stores the choice,
    // with a path variable where one applies.
    void browse(const QModelIndex &index)
    {
        if (!index.isValid())
            return;
        const SettingsSchemaEntry entry = schemaEntry(index);
        const bool folder = entry.kind == SettingsSchemaEntry::Kind::Folder;
        if (!folder && entry.kind != SettingsSchemaEntry::Kind::File)
            return;

        const QString current  = index.data(Qt::EditRole).toString().trimmed();
        const QString expanded = current.isEmpty() ? QString() : SettingsSchemaEntry::localPath(current);
        const QString path     = index.data(SettingsTreeModel::FullPathRole).toString();

        // Start where the current value points, or at the nearest folder that exists.
        QString start = expanded;
        while (!start.isEmpty() && !QFileInfo::exists(start)) {
            const QString up = QFileInfo(start).absolutePath();
            start = (up == start) ? QString() : up;
        }

        QString picked;
        if (folder) {
            picked = QFileDialog::getExistingDirectory(m_view->window(),
                                                       QObject::tr("Select folder: %1").arg(path),
                                                       start);
        } else {
            picked = QFileDialog::getOpenFileName(m_view->window(),
                                                  QObject::tr("Select file: %1").arg(path),
                                                  start, entry.fileFilter);
        }
        if (picked.isEmpty())
            return;

        QString value = DsEnvironment::contractPath(picked);
        // Keep a file: URL a URL ("file:///%APP%/data/clip.mp4").
        if (current.startsWith(u"file:", Qt::CaseInsensitive)) {
            if (value.startsWith(u"//"))
                value.prepend(QStringLiteral("file:")); // UNC: the host goes in the URL
            else if (value.startsWith(u'/'))
                value.prepend(QStringLiteral("file://"));
            else
                value.prepend(QStringLiteral("file:///"));
        }
        // Keep the folder style of the current value ("%LOCAL%/cache/").
        if (folder && (current.endsWith(u'/') || current.endsWith(u'\\')) && !value.endsWith(u'/'))
            value += u'/';
        if (value != current)
            m_view->model()->setData(index, value, Qt::EditRole);
    }

    void paintPath(QPainter *painter,
                   const QStyleOptionViewItem &opt,
                   const SettingsSchemaEntry &entry,
                   const QString &text,
                   const Colors &colors) const
    {
        const QRect cell   = opt.rect;
        const QRect button = browseButtonRect(cell);
        const bool  hover  = opt.state & QStyle::State_MouseOver;

        // A path that does not exist is shown in a warning colour.
        QColor textColor = colors.text;
        if (entry.pathMissing) {
            const bool dark = colors.text.lightness() > 128;
            textColor = dark ? QColor(255, 153, 164) : QColor(196, 43, 28);
        }
        const QRect textRect(cell.left() + textMargin(), cell.top(),
                             button.left() - 6 - (cell.left() + textMargin()), cell.height());
        painter->setPen(textColor);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                          opt.fontMetrics.elidedText(text, Qt::ElideMiddle, textRect.width()));

        // The button: three dots, on a soft background while the row is hovered.
        if (hover) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(colors.groove);
            painter->drawRoundedRect(QRectF(button), 4, 4);
        }
        QColor dots = colors.text;
        dots.setAlphaF(hover ? 1.0f : 0.6f);
        painter->setPen(Qt::NoPen);
        painter->setBrush(dots);
        const QPointF centre = QRectF(button).center();
        for (int i = -1; i <= 1; ++i)
            painter->drawEllipse(centre + QPointF(i * 4.5, 0.5), 1.4, 1.4);
    }

    // Remembers the cell an editor is open on, until the editor is destroyed.
    QWidget *trackEditor(QWidget *editor, const QModelIndex &index) const
    {
        m_editing = index;
        QObject::connect(editor, &QObject::destroyed, m_view, [this, target = QPersistentModelIndex(index)] {
            if (m_editing == target) {
                m_editing = QPersistentModelIndex();
                m_view->viewport()->update();
            }
        });
        return editor;
    }

    QAbstractItemView    *m_view;
    QPersistentModelIndex m_drag;            // slider being dragged, if any
    QVariantAnimation    *m_switchAnimation = nullptr;
    QPersistentModelIndex m_switchIndex;     // switch whose knob is sliding
    mutable QPersistentModelIndex m_editing; // cell with an open editor, if any
};

// ── SettingsTypeDelegate ──────────────────────────────────────────────────────
// Column 2 (type): the type name, followed by a small warning sign for
// settings whose schema says a change only takes effect after the app
// restarts. The sign is an amber outline, and filled once the value has been
// changed at runtime: that change is waiting for a restart. The tooltip says
// the same in words. It is painted rather than drawn as the emoji, so its size
// and baseline match the text at any DPI.

class SettingsTypeDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        const auto entry = index.data(SettingsTreeModel::SchemaEntryRole).value<SettingsSchemaEntry>();
        if (!entry.restart) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        const QString text = opt.text;
        const QWidget *widget = opt.widget;
        QStyle *style = widget ? widget->style() : QApplication::style();
        opt.text.clear();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

        const QPalette::ColorGroup group = !(opt.state & QStyle::State_Enabled) ? QPalette::Disabled
                                           : (opt.state & QStyle::State_Active) ? QPalette::Normal
                                                                                : QPalette::Inactive;
        const bool selected = opt.state & QStyle::State_Selected;
        const QColor textColor = opt.palette.color(group, selected ? QPalette::HighlightedText
                                                                   : QPalette::Text);
        const bool pending =
            index.data(SettingsTreeModel::ProvenanceRole).toString() == QStringLiteral("override");

        // The type name, then the sign right after it, cut short if the column
        // is too narrow for both.
        const int margin   = style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, widget) + 1;
        const qreal size   = std::round(opt.fontMetrics.height() * 0.85);
        const int  gap     = 6;
        const int  room    = opt.rect.width() - 2 * margin - gap - int(size);
        const QString shown = opt.fontMetrics.elidedText(text, Qt::ElideRight, std::max(0, room));
        const QRect textRect(opt.rect.left() + margin, opt.rect.top(),
                             opt.fontMetrics.horizontalAdvance(shown), opt.rect.height());

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setFont(opt.font);
        painter->setPen(textColor);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, shown);

        const QRectF sign(textRect.right() + 1 + gap, opt.rect.center().y() + 0.5 - size / 2,
                          size, size);
        paintWarningSign(painter, sign, pending, textColor);
        painter->restore();
    }

private:
    // A rounded warning triangle with an exclamation mark, in amber.
    static void paintWarningSign(QPainter *painter, const QRectF &r, bool filled, const QColor &text)
    {
        // Amber that reads on both light and dark backgrounds.
        const bool   dark  = text.lightness() > 128; // light text: dark theme
        const QColor amber = dark ? QColor(252, 200, 60) : QColor(214, 140, 0);

        const qreal  pen = std::max(1.0, r.height() / 11.0);
        const QRectF t   = r.adjusted(pen / 2, pen / 2 + r.height() * 0.04, -pen / 2, -pen / 2);
        QPainterPath triangle;
        triangle.moveTo(t.center().x(), t.top());
        triangle.lineTo(t.right(), t.bottom());
        triangle.lineTo(t.left(), t.bottom());
        triangle.closeSubpath();

        painter->setPen(QPen(amber, pen, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(filled ? QBrush(amber) : QBrush(Qt::NoBrush));
        painter->drawPath(triangle);

        // The exclamation mark: a bar and a dot, centred in the lower part.
        const QColor mark = filled ? QColor(32, 24, 0) : amber;
        const qreal  x    = t.center().x();
        const qreal  w    = std::max(1.2, r.width() * 0.11);
        painter->setPen(QPen(mark, w, Qt::SolidLine, Qt::RoundCap));
        painter->drawLine(QPointF(x, t.top() + t.height() * 0.38),
                          QPointF(x, t.top() + t.height() * 0.66));
        painter->setPen(Qt::NoPen);
        painter->setBrush(mark);
        painter->drawEllipse(QPointF(x, t.top() + t.height() * 0.83), w * 0.62, w * 0.62);
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
    }

    // Applies the edited list; stays open if any row fails to parse.
    void accept() override
    {
        if (applyChanges())
            QDialog::accept();
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

    bool applyChanges()
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
            if (orig.isValid() && text == SettingsTreeModel::displayString(orig)) {
                list.append(orig);
                continue;
            }
            const QMetaType type = orig.isValid() ? orig.metaType() : mt;
            const QVariant parsed = type.isValid()
                                        ? SettingsTreeModel::parseDisplayString(text, type)
                                        : QVariant(text);
            if (!parsed.isValid()) {
                m_list->setCurrentItem(li);
                QMessageBox::warning(this, windowTitle(),
                                     tr("Item %1: \"%2\" is not a valid %3.")
                                         .arg(i)
                                         .arg(text, QString::fromUtf8(type.name())));
                return false;
            }
            list.append(parsed);
        }

        m_sf->setOverride(m_path, QVariant::fromValue(list));
        return true;
    }

    // Returns a sensible zero/empty default for the given type.
    static QVariant defaultValue(const QMetaType mt)
    {
        if (mt == QMetaType::fromType<bool>())      return false;
        if (mt == QMetaType::fromType<int>())       return 0;
        if (mt == QMetaType::fromType<qlonglong>()) return qlonglong{0};
        if (mt == QMetaType::fromType<double>())    return 0.0;
        if (mt == QMetaType::fromType<float>())     return 0.0f;
        if (mt == QMetaType::fromType<QColor>())    return QColor(Qt::black);
        // Points, sizes, rects, vectors, dates…: a default-constructed value
        // of the same type, so new items don't turn the list into strings.
        if (mt.isValid() && mt != QMetaType::fromType<QVariantList>()
            && mt != QMetaType::fromType<QVariantMap>())
            return QVariant(mt);
        return QString{};
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

// ─────────────────────────────────────────────────────────────────────────────

// ── SaveLocationDialog ───────────────────────────────────────────────────────
// Asks where Save writes the current tab's overrides: into the settings file in
// one of the folders settings are read from (DsEnvironment::settingsLocations()),
// or a file of the user's choosing (Custom). The viewer keeps one instance and
// prepares it while it is open, so Save shows it at once.

// Why `filePath` cannot be written, or an empty string if it can.
//
// This must not touch the disk: the settings folders are watched, and a probe
// file created there makes Settings reload every settings file (and the viewer
// rebuild every tree) - which once made the dialog take a second to appear. So
// it asks for the access rights instead. On Windows the read-only attribute
// says nothing about a folder, so the NTFS permission lookup is switched on to
// read the access control lists. QSaveFile writes a temporary file next to the
// target, so the folder (or the nearest one above it that exists, which mkpath
// creates the rest from) must be writable as well as an existing file.
// Safe to call from any thread.
// Why a Save choice cannot be used: a short status for its pill ("Read-Only")
// and the explanation for the pill's tooltip. Empty when it can be used.
struct Unusable {
    QString label;
    QString detail;
    bool isEmpty() const { return label.isEmpty(); }
};

static Unusable unwritableReason(const QString &filePath)
{
    // Without the NTFS lookup isWritable() reports the read-only attribute,
    // with it only the access rights, so ask both ways. The lookup is switched
    // on for the whole process, so two checks must not overlap.
    static QMutex mutex;
    const QMutexLocker lock(&mutex);
    const QFileInfo info(filePath);
    if (info.exists() && !info.isWritable())
        return {QObject::tr("Read-Only"), QObject::tr("The file is read-only.")};
#ifdef Q_OS_WIN
    QNtfsPermissionCheckGuard permissions;
#endif
    QString folder = info.absolutePath();
    while (!QFileInfo(folder).isDir()) {
        const QString parent = QFileInfo(folder).absolutePath(); // QDir::cdUp() needs it to exist
        if (parent == folder)
            return {QObject::tr("Unavailable"), QObject::tr("No folder on this path exists.")};
        folder = parent;
    }
    if (!QFileInfo(folder).isWritable())
        return {QObject::tr("Read-Only"), QObject::tr("You cannot write to the folder %1.")
                                              .arg(QDir::toNativeSeparators(folder))};
    if (info.exists() && !QFileInfo(filePath).isWritable()) // fresh: no cached answer
        return {QObject::tr("Read-Only"), QObject::tr("You cannot write to the file.")};
    return {};
}

// The icon font Windows ships: Segoe Fluent Icons (Windows 11) or Segoe MDL2
// Assets (Windows 10). Empty where neither is installed.
static QString iconFontFamily()
{
    static const QString family = [] {
        for (const char *name : {"Segoe Fluent Icons", "Segoe MDL2 Assets"}) {
            if (QFontDatabase::hasFamily(QString::fromLatin1(name)))
                return QString::fromLatin1(name);
        }
        return QString();
    }();
    return family;
}

// Glyphs of that font.
namespace Glyph {
constexpr char16_t Save = 0xE74E;
constexpr char16_t Monitor = 0xE7F4;
constexpr char16_t Folder = 0xE8B7;
constexpr char16_t Document = 0xE8A5;
constexpr char16_t OpenFolder = 0xE838;
constexpr char16_t Info = 0xE946;
} // namespace Glyph

// A monochrome icon in the text colour, so it follows the theme and greys out
// with its card: a glyph of the system icon font, or the style's own icon
// where that font is missing.
static void setGlyph(QLabel *label, char16_t glyph, QStyle::StandardPixmap fallback, int size)
{
    const QString family = iconFontFamily();
    if (family.isEmpty()) {
        label->setPixmap(label->style()->standardIcon(fallback).pixmap(size, size));
        return;
    }
    QFont font(family);
    font.setPixelSize(size);
    label->setFont(font);
    label->setObjectName(QStringLiteral("glyph"));
    label->setText(QString(QChar(glyph)));
}

static void setGlyph(QToolButton *button, char16_t glyph, QStyle::StandardPixmap fallback, int size)
{
    const QString family = iconFontFamily();
    if (family.isEmpty()) {
        button->setIcon(button->style()->standardIcon(fallback));
        button->setIconSize({size, size});
        return;
    }
    QFont font(family);
    font.setPixelSize(size);
    button->setFont(font);
    button->setText(QString(QChar(glyph)));
}

// "1 value" or "4 values". Qt's %n plurals only resolve once a translation is
// loaded, so English spells out both forms.
static QString countOf(int n, const QString &one, const QString &many)
{
    return n == 1 ? one : many.arg(n);
}

// The location chosen last, by name ("Local Platform", "Custom"), and the last
// custom file; both are kept for as long as the application runs.
static QString s_lastSaveLocation;
static QString s_lastCustomPath;

// A choice in the Save dialog: clicking anywhere on it picks it.
class ChoiceCard : public QFrame
{
public:
    explicit ChoiceCard(QRadioButton *radio)
        : m_radio(radio)
    {
        setObjectName(QStringLiteral("choiceCard"));
        setCursor(Qt::PointingHandCursor);
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (m_radio->isEnabled())
            m_radio->click();
        QFrame::mousePressEvent(event);
    }

private:
    QRadioButton *m_radio;
};

// A path that is shortened in the middle when it does not fit, so the drive
// and the file name stay visible. The full path is in the tooltip.
class PathLabel : public QLabel
{
public:
    PathLabel() { setMinimumWidth(120); }

    void setPath(const QString &path)
    {
        m_path = path;
        setToolTip(path);
        elide();
    }

    QSize sizeHint() const override
    {
        // Long paths are shortened rather than widening the dialog.
        const int width = std::min(fontMetrics().horizontalAdvance(m_path) + 2, 560);
        return {width, QLabel::sizeHint().height()};
    }
    QSize minimumSizeHint() const override { return {120, QLabel::minimumSizeHint().height()}; }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QLabel::resizeEvent(event);
        elide();
    }

private:
    void elide() { QLabel::setText(fontMetrics().elidedText(m_path, Qt::ElideMiddle, width())); }

    QString m_path;
};

class SaveLocationDialog : public QDialog
{
public:
    static inline const QString customName = QStringLiteral("Custom");

    explicit SaveLocationDialog(QWidget *parent)
        : QDialog(parent)
    {
        setStyleSheet(styleSheetFor(palette()));
        setMinimumWidth(720);

        // ── Header: icon, title and what the list means ────────────────────
        auto *icon = new QLabel;
        setGlyph(icon, Glyph::Save, QStyle::SP_DialogSaveButton, 34);
        icon->setAlignment(Qt::AlignTop);
        m_title = new QLabel;
        m_title->setObjectName(QStringLiteral("dialogTitle"));
        m_subtitle = new QLabel;
        m_subtitle->setObjectName(QStringLiteral("muted"));
        m_subtitle->setWordWrap(true);
        auto *titles = new QVBoxLayout;
        titles->setSpacing(2);
        titles->addWidget(m_title);
        titles->addWidget(m_subtitle);
        auto *header = new QHBoxLayout;
        header->setSpacing(14);
        header->addWidget(icon);
        header->addLayout(titles, 1);

        auto *rule = new QFrame;
        rule->setObjectName(QStringLiteral("rule"));
        rule->setFixedHeight(1);

        auto *heading = new QLabel(tr("Settings locations"));
        heading->setObjectName(QStringLiteral("sectionTitle"));

        // ── One card per location, then Custom ─────────────────────────────
        auto *cards = new QVBoxLayout;
        cards->setSpacing(8);
        QStringList names;
        for (const DsEnvironment::SettingsLocation &location : DsEnvironment::settingsLocations())
            names.append(location.name);
        names.append(customName);
        for (const QString &name : std::as_const(names)) {
            cards->addWidget(makeCard(name));
        }
        Row &custom = m_rows.last();
        custom.path->setPath(tr("Choose a different .toml file"));

        connect(&m_group, &QButtonGroup::buttonClicked, this, [this] { m_userPicked = true; });
        connect(&m_group, &QButtonGroup::buttonToggled, this, [this] {
            markSelected();
            updateRemoval();
        });

        // Shown when files below the chosen one set values that changed: they
        // would keep overriding what is saved.
        m_remove = new QCheckBox;
        m_remove->setHidden(true);

        // ── Footer: how existing files are treated, and the buttons ────────
        auto *infoIcon = new QLabel;
        setGlyph(infoIcon, Glyph::Info, QStyle::SP_MessageBoxInformation, 16);
        auto *info = new QLabel(tr("Existing files keep their comments and formatting; "
                                   "only the values that changed are written."));
        info->setObjectName(QStringLiteral("muted"));
        info->setWordWrap(true);
        auto *infoRow = new QHBoxLayout;
        infoRow->setSpacing(8);
        infoRow->addWidget(infoIcon, 0, Qt::AlignTop);
        infoRow->addWidget(info, 1);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        buttons->button(QDialogButtonBox::Ok)->setText(tr("Save here"));
        buttons->button(QDialogButtonBox::Ok)->setObjectName(QStringLiteral("primary"));
        buttons->button(QDialogButtonBox::Cancel)->setObjectName(QStringLiteral("secondary"));
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(20, 18, 20, 16);
        layout->setSpacing(12);
        layout->addLayout(header);
        layout->addWidget(rule);
        layout->addWidget(heading);
        layout->addLayout(cards);
        layout->addWidget(m_remove);
        layout->addLayout(infoRow);
        layout->addWidget(buttons);
    }

    // Sets the dialog up for the settings file `fileName`, shown as `title`
    // ("App Settings"). Uses what earlier checks found right away and checks
    // again in the background, updating the choices when that is done.
    // `changedKeys` are the dotted keys of the tab's overrides: each card counts
    // the ones its file sets.
    void prepare(const QString &title, const QString &fileName, const QStringList &changedKeys)
    {
        m_keys = changedKeys;
        m_remove->setChecked(false);
        setWindowTitle(tr("Save %1").arg(title));
        m_title->setText(tr("Save %1").arg(title));
        m_subtitle->setText(tr("Select where the %1 settings are saved. "
                               "Files lower in the list override files above them.")
                                .arg(title));
        m_userPicked = false;
        if (fileName != m_fileName) {
            m_fileName = fileName;
            m_customPath.clear();
            m_rows.last().path->setPath(tr("Choose a different .toml file"));
        }

        const QList<DsEnvironment::SettingsLocation> locations = DsEnvironment::settingsLocations();
        for (Row &row : m_rows) {
            const auto location = std::find_if(locations.cbegin(), locations.cend(),
                                               [&row](const auto &l) { return l.name == row.name; });
            if (location == locations.cend())
                continue; // Custom
            row.file.clear();
            row.fixedReason = {};
            if (location->folder.isEmpty()) {
                static const QRegularExpression variable(QStringLiteral("%[A-Z_]+%"));
                QStringList unset;
                for (const auto &match : variable.globalMatch(DsEnvironment::expandq(location->pattern)))
                    unset.append(match.captured());
                row.path->setPath(QDir::toNativeSeparators(location->pattern + u'/' + fileName));
                row.fixedReason = {tr("Not Set"),
                                   tr("%1 is not set, so this folder is not used.")
                                       .arg(unset.join(QStringLiteral(" and ")))};
            } else {
                row.file = QDir(location->folder).filePath(fileName);
                row.path->setPath(QDir::toNativeSeparators(row.file));
                // A folder whose variables are set is a search path: the locations
                // are the ones loadEngineSettings() searches.
                if (settings::isResourcePath(location->folder))
                    row.fixedReason = {tr("Built-In"),
                                       tr("These settings are built into the application.")};
            }
        }
        refresh();
        probe();
    }

    // The name of the chosen location: one of DsEnvironment::settingsLocations()'s, or customName.
    QString chosenName() const
    {
        for (const Row &row : m_rows)
            if (row.radio->isChecked())
                return row.name;
        return {};
    }

    // A file below the chosen one that sets values that changed, with those
    // keys and their values as written there.
    struct Removal {
        QString name; // the location, "Local Platform"
        QString file;
        QList<std::pair<QString, QString>> keys;
    };

    // What to remove from the files below the chosen one, if its checkbox is ticked.
    QList<Removal> removals() const
    {
        if (m_remove->isHidden() || !m_remove->isEnabled() || !m_remove->isChecked())
            return {};
        return m_removals;
    }

    // The file to write. For Custom, the file picked with its browse button,
    // or an empty string if none was.
    QString chosenFile() const
    {
        for (const Row &row : m_rows)
            if (row.radio->isChecked())
                return row.name == customName ? m_customPath : row.file;
        return {};
    }

private:
    struct Row {
        QString name;
        ChoiceCard *card = nullptr;
        QRadioButton *radio = nullptr;
        QLabel *icon = nullptr;
        QLabel *title = nullptr;
        QLabel *status = nullptr;
        PathLabel *path = nullptr;
        // What saving here does with the changed values; not for Custom.
        QLabel *updates = nullptr;    // this file sets them already
        QLabel *overrides = nullptr;  // a file above sets them; saved here, they win
        QLabel *overridden = nullptr; // a file below sets them; that file keeps winning
        QString file;        // empty for Custom and for a folder that is not known
        Unusable fixedReason; // why the choice does not apply, known without touching the disk
    };
    struct Check {
        Unusable reason; // why the file cannot be written; empty if it can
        bool exists = false;
        QStringList keys;                       // the changed keys looked for
        QList<std::pair<QString, QString>> set; // those the file sets, with their values
    };

    // Colours derived from the palette, so the dialog follows a light or dark theme.
    static QString styleSheetFor(const QPalette &palette)
    {
        const QColor window = palette.color(QPalette::Window);
        const QColor text = palette.color(QPalette::WindowText);
        const QColor accent = palette.color(QPalette::Highlight);
        const bool dark = window.lightness() < 128;
        const auto rgba = [](const QColor &c, double alpha) {
            return QStringLiteral("rgba(%1, %2, %3, %4)")
                .arg(c.red()).arg(c.green()).arg(c.blue()).arg(alpha);
        };
        const QColor green = dark ? QColor(102, 187, 106) : QColor(46, 125, 50);
        const QColor amber = dark ? QColor(255, 183, 77) : QColor(178, 106, 0);
        const QColor red = dark ? QColor(239, 83, 80) : QColor(198, 40, 40);
        const QColor accentText = dark ? accent.lighter(135) : accent.darker(120);
        return QStringLiteral(R"(
            QLabel#dialogTitle { font-size: 15pt; font-weight: 600; }
            QLabel#sectionTitle { font-size: 11pt; font-weight: 600; }
            QLabel#muted, QLabel#path { color: %1; }
            QFrame#rule { background: %2; }
            QFrame#choiceCard { background: %3; border: 1px solid %2; border-radius: 8px; }
            QFrame#choiceCard[selected="true"] { background: %4; border: 1px solid %5; }
            QLabel#cardTitle { font-size: 11pt; font-weight: 600; }
            QLabel#path:disabled, QLabel#cardTitle:disabled, QLabel#badge:disabled { color: %6; }
            QLabel#badge, QLabel#pill, QLabel#chip {
                border-radius: 4px; padding: 1px 7px; font-size: 8pt; font-weight: 600; }
            QLabel#badge { background: %7; color: %1; }
            QLabel#chip[kind="updates"] { background: %15; border: 1px solid %16; color: %17; }
            QLabel#chip[kind="overrides"] { background: %9; border: 1px solid %10; color: %11; }
            QLabel#chip[kind="overridden"] { background: %18; border: 1px solid %19; color: %20; }
            QLabel#glyph:disabled { color: %6; }
            QToolButton#browse { color: %21; background: %3; border: 1px solid %6;
                                 border-radius: 5px; padding: 4px 7px; }
            QToolButton#browse:hover { background: %2; }
            QLabel#pill { font-size: 9pt; padding: 2px 9px; border: 1px solid %2; color: %1; }
            QLabel#pill[kind="update"] { background: %9; border-color: %10; color: %11; }
            QLabel#pill[kind="new"] { background: %12; border-color: %5; color: %8; }
            QPushButton#primary, QPushButton#secondary {
                border-radius: 5px; padding: 6px 22px; min-width: 70px; }
            QPushButton#primary { background: %5; border: 1px solid %5; color: %13; font-weight: 600; }
            QPushButton#primary:hover { background: %14; }
            QPushButton#secondary { background: %3; border: 1px solid %6; }
            QPushButton#secondary:hover { background: %2; }
        )")
            .arg(rgba(text, 0.6), rgba(text, 0.14), rgba(text, 0.035), rgba(accent, 0.14),
                 accent.name(), rgba(text, 0.35), rgba(text, 0.09), accentText.name())
            .arg(rgba(green, 0.14), rgba(green, 0.55), green.name(), rgba(accent, 0.12),
                 palette.color(QPalette::HighlightedText).name(), accent.lighter(112).name())
            .arg(rgba(amber, 0.14), rgba(amber, 0.55), amber.name(), rgba(red, 0.14),
                 rgba(red, 0.55), red.name(), text.name());
    }

    QWidget *makeCard(const QString &name)
    {
        Row row;
        row.name = name;
        row.radio = new QRadioButton;
        row.radio->setAccessibleName(name);
        row.card = new ChoiceCard(row.radio);

        const bool custom = name == customName;
        const bool installed = name.startsWith(QStringLiteral("Application"));
        row.icon = new QLabel;
        if (custom)
            setGlyph(row.icon, Glyph::Document, QStyle::SP_FileIcon, 22);
        else if (installed)
            setGlyph(row.icon, Glyph::Monitor, QStyle::SP_ComputerIcon, 22);
        else
            setGlyph(row.icon, Glyph::Folder, QStyle::SP_DirIcon, 22);

        row.title = new QLabel(custom ? tr("Custom file…") : name);
        row.title->setObjectName(QStringLiteral("cardTitle"));
        // Where the file lives: installed with the application, this machine's
        // own, or anywhere.
        auto *badge = new QLabel(custom ? tr("CUSTOM") : installed ? tr("INSTALLED") : tr("THIS MACHINE"));
        badge->setObjectName(QStringLiteral("badge"));
        badge->setToolTip(custom ? tr("A file of your choosing")
                          : installed ? tr("Installed with the application; shared by every machine it is installed on")
                                      : tr("Kept outside the installation, for this machine only"));

        auto *titleRow = new QHBoxLayout;
        titleRow->setSpacing(8);
        titleRow->addWidget(row.title);
        titleRow->addWidget(badge);
        titleRow->addStretch();

        row.path = new PathLabel;
        row.path->setObjectName(QStringLiteral("path"));

        auto *text = new QVBoxLayout;
        text->setSpacing(3);
        text->addLayout(titleRow);
        text->addWidget(row.path);
        if (!custom) {
            auto *chips = new QHBoxLayout;
            chips->setSpacing(6);
            for (auto [chip, kind] : {std::pair{&row.updates, "updates"},
                                      std::pair{&row.overrides, "overrides"},
                                      std::pair{&row.overridden, "overridden"}}) {
                *chip = new QLabel;
                (*chip)->setObjectName(QStringLiteral("chip"));
                (*chip)->setProperty("kind", QString::fromLatin1(kind));
                (*chip)->setHidden(true);
                chips->addWidget(*chip);
            }
            chips->addStretch();
            text->addLayout(chips);
        }

        auto *layout = new QHBoxLayout(row.card);
        layout->setContentsMargins(14, 10, 14, 10);
        layout->setSpacing(12);
        layout->addWidget(row.radio);
        layout->addWidget(row.icon);
        layout->addLayout(text, 1);

        if (custom) {
            auto *browse = new QToolButton;
            browse->setObjectName(QStringLiteral("browse"));
            setGlyph(browse, Glyph::OpenFolder, QStyle::SP_DirOpenIcon, 18);
            browse->setToolTip(tr("Choose the file…"));
            connect(browse, &QToolButton::clicked, this, [this] { browseCustom(); });
            layout->addWidget(browse);
        } else {
            row.status = new QLabel;
            row.status->setObjectName(QStringLiteral("pill"));
            layout->addWidget(row.status, 0, Qt::AlignTop);
        }

        m_group.addButton(row.radio);
        m_rows.append(row);
        return row.card;
    }

    void browseCustom()
    {
        const QString start = !m_customPath.isEmpty() ? m_customPath
                              : s_lastCustomPath.isEmpty()
                                  ? m_fileName
                                  : QFileInfo(s_lastCustomPath).dir().filePath(m_fileName);
        // The file is edited, not replaced, so there is no overwrite to confirm.
        const QString path = QFileDialog::getSaveFileName(this, tr("Save overrides"), start,
                                                          tr("TOML files (*.toml);;All files (*)"),
                                                          nullptr, QFileDialog::DontConfirmOverwrite);
        if (path.isEmpty())
            return;
        m_customPath = path;
        s_lastCustomPath = path;
        m_rows.last().path->setPath(QDir::toNativeSeparators(path));
        m_rows.last().radio->setChecked(true);
        m_userPicked = true;
    }

    static void setKind(QLabel *label, const char *kind)
    {
        label->setProperty("kind", QString::fromLatin1(kind));
        label->style()->unpolish(label);
        label->style()->polish(label);
    }

    // Shows what is known about each choice and selects one.
    void refresh()
    {
        for (Row &row : m_rows) {
            if (row.name == customName)
                continue;
            Unusable reason = row.fixedReason;
            bool known = true;
            bool exists = false;
            if (reason.isEmpty()) {
                const auto check = m_checks.constFind(row.file);
                known = check != m_checks.cend();
                if (known) {
                    reason = check->reason;
                    exists = check->exists;
                }
            }
            const bool usable = known && reason.isEmpty();
            row.card->setEnabled(usable);
            if (!known) {
                row.status->setText(tr("Checking…"));
                row.status->setToolTip(tr("Checking whether this file can be written."));
                setKind(row.status, "");
            } else if (!reason.isEmpty()) {
                row.status->setText(reason.label);
                row.status->setToolTip(reason.detail);
                setKind(row.status, "");
            } else if (exists) {
                row.status->setText(tr("Existing"));
                row.status->setToolTip(tr("The file exists. Only the values that changed are "
                                          "written into it; the rest is kept as it is."));
                setKind(row.status, "update");
            } else {
                row.status->setText(tr("New"));
                row.status->setToolTip(tr("The file does not exist yet; saving creates it."));
                setKind(row.status, "new");
            }
            // A disabled card shows no tooltips, so the reason goes on the card too.
            row.card->setToolTip(usable ? QString() : row.status->toolTip());
        }
        updateChips();
        QAbstractButton *checked = m_group.checkedButton();
        if (!m_userPicked || !checked || !checked->isEnabled())
            selectDefault();
        markSelected();
        updateRemoval();
    }

    // The changed keys `row`'s file sets, with their values; null while unknown.
    const QList<std::pair<QString, QString>> *keysSetBy(const Row &row) const
    {
        if (row.file.isEmpty())
            return nullptr;
        const auto found = m_checks.constFind(row.file);
        if (found == m_checks.cend() || found->keys != m_keys)
            return nullptr;
        return &found->set;
    }

    // The value `key` has in `row`'s file, or an empty string if it does not set it.
    QString valueIn(const Row &row, const QString &key) const
    {
        if (const auto *set = keysSetBy(row)) {
            for (const auto &[k, value] : *set)
                if (k == key)
                    return value;
        }
        return {};
    }

    // Sorts each changed value into one chip per card: Overridden by when a
    // file below sets it (saving here then has no effect), else Updates when
    // this file sets it, else Overrides when a file above does.
    void updateChips()
    {
        for (qsizetype i = 0; i < m_rows.size(); ++i) {
            Row &row = m_rows[i];
            if (!row.updates)
                continue; // Custom
            QStringList updates, overrides, overridden;
            if (row.card->isEnabled()) {
                for (const QString &key : std::as_const(m_keys)) {
                    // The last file below that sets it is the one that wins.
                    QString below;
                    for (qsizetype j = i + 1; j < m_rows.size(); ++j) {
                        if (const QString value = valueIn(m_rows[j], key); !value.isEmpty())
                            below = QStringLiteral("    %1 = %2 (%3)").arg(key, value, m_rows[j].name);
                    }
                    if (!below.isEmpty()) {
                        overridden.append(below);
                        continue;
                    }
                    if (const QString value = valueIn(row, key); !value.isEmpty()) {
                        updates.append(QStringLiteral("    %1 = %2").arg(key, value));
                        continue;
                    }
                    // The last file above that sets it is the one saving here beats.
                    QString above;
                    for (qsizetype j = 0; j < i; ++j) {
                        if (const QString value = valueIn(m_rows[j], key); !value.isEmpty())
                            above = QStringLiteral("    %1 = %2 (%3)").arg(key, value, m_rows[j].name);
                    }
                    if (!above.isEmpty())
                        overrides.append(above);
                }
            }
            const auto show = [](QLabel *chip, const QStringList &lines, const QString &text,
                                 const QString &heading) {
                chip->setHidden(lines.isEmpty());
                chip->setText(text.arg(lines.size()));
                chip->setToolTip(heading + u'\n' + lines.join(u'\n'));
            };
            show(row.updates, updates, tr("Updates %1"),
                 tr("This file already sets these changed values; saving here changes them:"));
            show(row.overrides, overrides, tr("Overrides %1"),
                 tr("Files above set these changed values; saved here, yours take their place:"));
            show(row.overridden, overridden, tr("Overridden by %1"),
                 tr("Files below set these changed values, so they keep overriding what you "
                    "save here:"));
        }
    }

    // "A (2), B (1) and C (1)"
    static QString joinNames(const QStringList &parts)
    {
        if (parts.size() < 2)
            return parts.join(QString());
        return parts.mid(0, parts.size() - 1).join(QStringLiteral(", ")) + tr(" and ") + parts.last();
    }

    // Offers to remove the changed values from the files below the chosen one,
    // which would otherwise keep overriding what is saved.
    void updateRemoval()
    {
        m_removals.clear();
        qsizetype chosen = -1;
        for (qsizetype i = 0; i < m_rows.size(); ++i)
            if (m_rows[i].radio->isChecked())
                chosen = i;
        int total = 0;
        QStringList parts;
        QStringList details;
        QStringList blocked;
        if (chosen >= 0 && m_rows[chosen].name != customName) {
            for (qsizetype i = chosen + 1; i < m_rows.size(); ++i) {
                const Row &row = m_rows[i];
                if (row.name == customName || row.file.isEmpty())
                    continue;
                const auto found = m_checks.constFind(row.file);
                if (found == m_checks.cend() || found->keys != m_keys || found->set.isEmpty())
                    continue;
                m_removals.append({row.name, row.file, found->set});
                total += int(found->set.size());
                parts.append(QStringLiteral("%1 (%2)").arg(row.name).arg(found->set.size()));
                details.append(row.name + u':');
                for (const auto &[key, value] : found->set)
                    details.append(QStringLiteral("    %1 = %2").arg(key, value));
                if (!found->reason.isEmpty())
                    blocked.append(QStringLiteral("%1: %2").arg(row.name, found->reason.detail));
            }
        }
        m_remove->setHidden(m_removals.isEmpty());
        if (m_removals.isEmpty()) {
            m_remove->setChecked(false);
            return;
        }
        m_remove->setText(tr("Also remove %1 from %2")
                              .arg(countOf(total, tr("1 value"), tr("%1 values")), joinNames(parts)));
        QString tip = tr("These files come later and set values you changed, so they would "
                         "keep overriding what you save:")
                      + u'\n' + details.join(u'\n');
        if (!blocked.isEmpty())
            tip += QStringLiteral("\n\n") + tr("They cannot be removed:") + u'\n' + blocked.join(u'\n');
        m_remove->setToolTip(tip);
        m_remove->setEnabled(blocked.isEmpty());
        if (!blocked.isEmpty())
            m_remove->setChecked(false);
    }

    // Highlights the card of the checked choice.
    void markSelected()
    {
        for (const Row &row : m_rows) {
            const bool selected = row.radio->isChecked();
            if (row.card->property("selected").toBool() == selected)
                continue;
            row.card->setProperty("selected", selected);
            row.card->style()->unpolish(row.card);
            row.card->style()->polish(row.card);
        }
    }

    // The choice made last if it still applies; otherwise the least far-reaching
    // one: a machine's own platform files before the installed files that every
    // machine shares.
    void selectDefault()
    {
        const auto find = [this](const QString &name) -> QRadioButton * {
            for (const Row &row : m_rows)
                if (row.name == name && row.radio->isEnabled())
                    return row.radio;
            return nullptr;
        };
        QRadioButton *pick = find(s_lastSaveLocation);
        for (const char *name : {"Local Platform", "Local", "Application Platform", "Application"}) {
            if (!pick)
                pick = find(QString::fromLatin1(name));
        }
        if (!pick)
            pick = find(customName);
        pick->setChecked(true);
    }

    // Checks the files on a worker thread; refresh() shows the result.
    void probe()
    {
        QStringList files;
        for (const Row &row : m_rows)
            if (!row.file.isEmpty() && row.fixedReason.isEmpty())
                files.append(row.file);
        if (files.isEmpty())
            return;
        const int generation = ++m_generation;
        const QStringList keys = m_keys;
        QPointer<SaveLocationDialog> self(this);
        QThreadPool::globalInstance()->start([self, files, keys, generation] {
            QHash<QString, Check> results;
            for (const QString &file : files) {
                Check check{unwritableReason(file), QFileInfo::exists(file), keys, {}};
                if (check.exists && !keys.isEmpty()) {
                    QFile in(file);
                    if (in.open(QIODevice::ReadOnly))
                        check.set = settings::tomlKeysSet(QString::fromUtf8(in.readAll()), keys);
                }
                results.insert(file, check);
            }
            // Back on the GUI thread; `self` is only dereferenced there.
            QMetaObject::invokeMethod(
                QCoreApplication::instance(),
                [self, results, generation] {
                    if (!self || generation != self->m_generation)
                        return; // closed, or a newer check is on its way
                    self->m_checks.insert(results);
                    self->refresh();
                },
                Qt::QueuedConnection);
        });
    }

    QLabel *m_title = nullptr;
    QLabel *m_subtitle = nullptr;
    QButtonGroup m_group;
    QList<Row> m_rows;
    QHash<QString, Check> m_checks; // by file, kept so the next prepare() can show them at once
    QString m_fileName;
    QString m_customPath;
    QStringList m_keys;        // the changed keys, dotted
    QCheckBox *m_remove = nullptr;
    QList<Removal> m_removals; // what the checkbox would remove
    int m_generation = 0;
    bool m_userPicked = false;
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
    // Keep the Save dialog ready for the current tab while the viewer is open.
    connect(m_tabs, &QTabWidget::currentChanged, this, [this] {
        if (isVisible())
            prepareSaveDialog();
    });
    connect(settings, &Settings::searchPathsChanged, this, [this] {
        if (isVisible())
            prepareSaveDialog();
    });
    connect(saveBtn,    &QPushButton::clicked, this, &SettingsViewerWidget::onSave);
    connect(restoreBtn, &QPushButton::clicked, this, &SettingsViewerWidget::onRestore);
    rebuild();
}

void SettingsViewerWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    prepareSaveDialog();
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
        proxy->setSourceModel(model);
        view->setModel(proxy);
        view->setItemDelegateForColumn(1, new SettingsValueDelegate(view));
        view->setItemDelegateForColumn(2, new SettingsTypeDelegate(view));
        view->setAlternatingRowColors(true);
        view->setUniformRowHeights(true);
        view->setRootIsDecorated(true);
        view->setContextMenuPolicy(Qt::CustomContextMenu);

        // Expand after every model reset (file reload, etc.).
        connect(model, &SettingsTreeModel::modelReset, view, &QTreeView::expandAll);
        view->expandAll();

        auto *searchBox = new QLineEdit;
        searchBox->setPlaceholderText(tr("Filter by key, path, or value…"));
        searchBox->setClearButtonEnabled(true);
        connect(searchBox, &QLineEdit::textChanged, view, [proxy, view](const QString &text) {
            proxy->setFilterKeyword(text);
            view->expandAll(); // reveal matches even under previously-collapsed branches
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

// The dotted keys of the leaves of a nested map: {"a": {"b": 1}} -> "a.b".
static QStringList leafKeys(const QVariantMap &map, const QString &prefix = {})
{
    QStringList keys;
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        const QString key = prefix.isEmpty() ? it.key() : prefix + u'.' + it.key();
        if (it.value().metaType() == QMetaType::fromType<QVariantMap>())
            keys += leafKeys(it.value().toMap(), key);
        else
            keys.append(key);
    }
    return keys;
}

void SettingsViewerWidget::prepareSaveDialog()
{
    auto *sf = currentSettingsFile();
    if (!sf)
        return;
    if (!m_saveDialog) {
        m_saveDialog = new SaveLocationDialog(this);
        m_saveDialog->winId(); // create the native window now rather than on Save
    }
    static_cast<SaveLocationDialog *>(m_saveDialog)
        ->prepare(m_tabs->tabText(m_tabs->currentIndex()), QFileInfo(sf->fileName()).fileName(),
                  leafKeys(sf->overrides()));
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

    const QString fileName = QFileInfo(sf->fileName()).fileName();
    prepareSaveDialog();
    auto *dialog = static_cast<SaveLocationDialog *>(m_saveDialog);
    if (dialog->exec() != QDialog::Accepted)
        return;
    s_lastSaveLocation = dialog->chosenName();

    QString path = dialog->chosenFile();
    if (path.isEmpty()) {
        const QString start = s_lastCustomPath.isEmpty()
                                  ? fileName
                                  : QFileInfo(s_lastCustomPath).dir().filePath(fileName);
        // The file is edited, not replaced, so there is no overwrite to confirm.
        path = QFileDialog::getSaveFileName(this, tr("Save overrides"), start,
                                            tr("TOML files (*.toml);;All files (*)"), nullptr,
                                            QFileDialog::DontConfirmOverwrite);
        if (path.isEmpty())
            return;
        s_lastCustomPath = path;
    }

    // A file reload while the dialog was open drops overrides that the files now hold.
    if (sf->overrides().isEmpty()) {
        QMessageBox::information(this, tr("Save settings"),
                                 tr("There are no overrides to save any more: the settings "
                                    "files already hold these values."));
        return;
    }

    // Removing values from other files cannot be undone from here: confirm it,
    // saying how far it reaches.
    const QList<SaveLocationDialog::Removal> removals = dialog->removals();
    if (!removals.isEmpty()) {
        int count = 0;
        QStringList lines;
        QStringList installed;
        for (const SaveLocationDialog::Removal &removal : removals) {
            count += int(removal.keys.size());
            lines.append(removal.name + u':');
            for (const auto &[key, value] : removal.keys)
                lines.append(QStringLiteral("    %1 = %2").arg(key, value));
            if (removal.name.startsWith(QStringLiteral("Application")))
                installed.append(removal.name);
        }
        const int writes = int(leafKeys(sf->overrides()).size());
        QString details = lines.join(u'\n') + QStringLiteral("\n\n")
                          + tr("The removed values cannot be restored from here.");
        if (!installed.isEmpty())
            details += QStringLiteral("\n\n")
                       + tr("%1 is installed with the application: removing values there "
                            "changes every machine that uses this installation.")
                             .arg(installed.join(tr(" and ")));
        QMessageBox box(QMessageBox::Warning, tr("Save and Remove"),
                        tr("This writes %1 to %2 and removes %3 from %4.")
                            .arg(countOf(writes, tr("1 value"), tr("%1 values")), dialog->chosenName(),
                                 countOf(count, tr("1 value"), tr("%1 values")),
                                 countOf(int(removals.size()), tr("1 other file"), tr("%1 other files"))),
                        QMessageBox::NoButton, this);
        box.setInformativeText(details);
        QAbstractButton *proceed = box.addButton(tr("Save and Remove"), QMessageBox::AcceptRole);
        box.setDefaultButton(box.addButton(QMessageBox::Cancel));
        box.exec();
        if (box.clickedButton() != proceed)
            return;
    }

    QStringList written;
    const QString err = sf->saveOverridesTo(path, &written);
    if (!err.isEmpty()) {
        QMessageBox::warning(this, tr("Save settings"), err);
        return;
    }

    QStringList report;
    QStringList problems;
    for (const SaveLocationDialog::Removal &removal : removals) {
        QStringList keys;
        for (const auto &[key, value] : removal.keys)
            keys.append(key);
        QStringList removed;
        const QString problem = settings::removeKeysFromFile(removal.file, keys, &removed);
        if (!problem.isEmpty())
            problems.append(problem);
        else if (!removed.isEmpty())
            report.append(tr("Removed %1 from %2:")
                              .arg(countOf(int(removed.size()), tr("1 value"), tr("%1 values")),
                                   QDir::toNativeSeparators(removal.file))
                          + u'\n' + removed.join(u'\n'));
    }

    // Read the files again now rather than when the file watcher notices: the
    // overrides the saved file now holds are dropped, so their values show as
    // coming from that file. (One saved below a file that still sets the same
    // key stays an override, as it should.)
    sf->reload();

    const QString where = QDir::toNativeSeparators(path);
    QStringList shown = written.mid(0, 12);
    if (written.size() > shown.size())
        shown.append(tr("… and %1 more").arg(written.size() - shown.size()));
    QStringList message{written.isEmpty()
                            ? tr("%1 already holds these values; nothing was written.").arg(where)
                            : tr("Wrote %1 to %2:")
                                      .arg(countOf(int(written.size()), tr("1 value"), tr("%1 values")), where)
                                  + u'\n' + shown.join(u'\n')};
    message += report;
    if (!problems.isEmpty()) {
        message.append(tr("Some values could not be removed:") + u'\n' + problems.join(u'\n'));
        QMessageBox::warning(this, tr("Save settings"), message.join(QStringLiteral("\n\n")));
        return;
    }
    QMessageBox::information(this, tr("Save settings"), message.join(QStringLiteral("\n\n")));
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

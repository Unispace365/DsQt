#include "core/dsToolWindow.h"

#include <QCoreApplication>
#include <QEvent>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QScreen>
#include <QSettings>
#include <QWidget>
#include <QWindow>

namespace dsqt {

namespace toolwindow {

namespace {

// Per user and per application, beside nothing the application itself
// configures: this is window furniture, not content.
QSettings store()
{
    return QSettings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("downstream"),
                     QCoreApplication::applicationName());
}

QString geometryKey(const QString &key)
{
    return QStringLiteral("ToolWindows/%1/geometry").arg(key);
}

// Whether enough of `rect` is on some screen to grab its title bar - a window
// saved on a monitor that is gone should not reappear out of reach.
bool reachable(const QRect &rect)
{
    for (const QScreen *screen : QGuiApplication::screens()) {
        const QRect overlap = screen->availableGeometry().intersected(rect);
        if (overlap.width() >= 100 && overlap.height() >= 50)
            return true;
    }
    return false;
}

} // namespace

QWindow *mainWindow(QObject *context)
{
    if (auto *engine = qobject_cast<QQmlApplicationEngine *>(qmlEngine(context))) {
        if (!engine->rootObjects().isEmpty())
            return qobject_cast<QWindow *>(engine->rootObjects().first());
    }
    return nullptr;
}

void makeTransient(QWidget *widget, QObject *context)
{
    QWindow *main = mainWindow(context);
    if (!widget || !main)
        return;
    widget->winId(); // creates the native window, so windowHandle() is valid
    if (QWindow *handle = widget->windowHandle())
        handle->setTransientParent(main);
}

bool restoreGeometry(QWidget *widget, const QString &key)
{
    if (!widget || key.isEmpty())
        return false;
    const QByteArray geometry = store().value(geometryKey(key)).toByteArray();
    // QWidget::restoreGeometry() already moves a window back onto a screen.
    return !geometry.isEmpty() && widget->restoreGeometry(geometry);
}

bool restoreGeometry(QWindow *window, const QString &key)
{
    if (!window || key.isEmpty())
        return false;
    const QRect rect = store().value(geometryKey(key)).toRect();
    if (!rect.isValid() || !reachable(rect))
        return false;
    window->setGeometry(rect);
    return true;
}

void saveGeometry(const QWidget *widget, const QString &key)
{
    if (widget && !key.isEmpty())
        store().setValue(geometryKey(key), widget->saveGeometry());
}

void saveGeometry(const QWindow *window, const QString &key)
{
    if (window && !key.isEmpty() && window->windowStates() == Qt::WindowNoState)
        store().setValue(geometryKey(key), window->geometry());
}

} // namespace toolwindow

// --- DsQmlToolWindowHelper --------------------------------------------------

DsQmlToolWindowHelper::DsQmlToolWindowHelper(const QString &key, QObject *parent)
    : QObject(parent)
    , m_key(key)
{}

DsQmlToolWindowHelper::~DsQmlToolWindowHelper()
{
    if (m_widget && m_widget->isVisible())
        toolwindow::saveGeometry(m_widget, m_key);
    delete m_widget;
}

void DsQmlToolWindowHelper::show()
{
    if (!m_widget) {
        m_widget = createWidget();
        if (!m_widget)
            return;
        m_widget->installEventFilter(this);
        // The system's own UI font, as standard dialogs use, rather than the
        // application's default font: engine.font.toml sizes that for the
        // full-screen content (12 pt), which makes a tool window needlessly
        // large. Item views already got the system font from Qt; now the
        // whole window matches.
        m_widget->setFont(QFontDatabase::systemFont(QFontDatabase::GeneralFont));
        toolwindow::makeTransient(m_widget, this);
        toolwindow::restoreGeometry(m_widget, m_key);
        // An open window is saved where it is when the application quits.
        connect(qApp, &QCoreApplication::aboutToQuit, this, [this] {
            if (m_widget && m_widget->isVisible())
                toolwindow::saveGeometry(m_widget, m_key);
        });
    }
    if (m_widget->isMinimized())
        m_widget->showNormal();
    else
        m_widget->show();
    m_widget->raise();
    m_widget->activateWindow();
}

void DsQmlToolWindowHelper::hide()
{
    if (m_widget)
        m_widget->hide();
}

void DsQmlToolWindowHelper::setVisible(bool visible)
{
    if (visible)
        show();
    else
        hide();
}

bool DsQmlToolWindowHelper::isVisible() const
{
    return m_widget && m_widget->isVisible();
}

bool DsQmlToolWindowHelper::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_widget) {
        if (event->type() == QEvent::Show) {
            emit visibleChanged(true);
        } else if (event->type() == QEvent::Hide) {
            // Minimising also sends a Hide; the window is still "open" then.
            if (!m_widget->isMinimized())
                toolwindow::saveGeometry(m_widget, m_key);
            emit visibleChanged(m_widget->isVisible());
        }
    }
    return QObject::eventFilter(watched, event);
}

// --- DsQmlToolWindows -------------------------------------------------------

DsQmlToolWindows::DsQmlToolWindows(QObject *parent)
    : QObject(parent)
{}

void DsQmlToolWindows::track(QWindow *window, const QString &key)
{
    if (!window)
        return;

    QWindow *main = toolwindow::mainWindow(this);
    if (main && main != window && window->transientParent() != main)
        window->setTransientParent(main);

    toolwindow::restoreGeometry(window, key);

    connect(window, &QWindow::visibleChanged, window, [window, key](bool visible) {
        if (!visible)
            toolwindow::saveGeometry(window, key);
    });
    connect(qApp, &QCoreApplication::aboutToQuit, window, [window, key] {
        if (window->isVisible())
            toolwindow::saveGeometry(window, key);
    });
}

} // namespace dsqt

#include "bridge/dsQmlContentBrowserHelper.h"
#include "bridge/dsContentBrowserWidget.h"

#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QWindow>

namespace dsqt::bridge {

namespace {

// Returns the application's main QML window (the root ApplicationWindow), or
// nullptr if it can't be resolved from this object's QML engine.
QWindow *mainQmlWindow(QObject *context)
{
    if (auto *engine = qobject_cast<QQmlApplicationEngine *>(qmlEngine(context))) {
        if (!engine->rootObjects().isEmpty())
            return qobject_cast<QWindow *>(engine->rootObjects().first());
    }
    return nullptr;
}

// Makes `widget`'s window a transient child of the main QML window, so it
// floats above the application like a dialog without forcing itself above
// unrelated top-level windows (as Qt::WindowStaysOnTopHint would).
void setTransientToMainWindow(QWidget *widget, QObject *context)
{
    QWindow *mainWindow = mainQmlWindow(context);
    if (!mainWindow)
        return;
    widget->winId(); // force-create the native handle so windowHandle() is valid
    if (QWindow *wh = widget->windowHandle())
        wh->setTransientParent(mainWindow);
}

} // namespace

DsQmlContentBrowserHelper::DsQmlContentBrowserHelper(QObject *parent)
    : QObject(parent)
{}

DsQmlContentBrowserHelper::~DsQmlContentBrowserHelper()
{
    delete m_viewer;
}

void DsQmlContentBrowserHelper::show()
{
    if (!m_viewer) {
        m_viewer = new DsContentBrowserWidget;
        connect(m_viewer, &DsContentBrowserWidget::visibilityChanged,
                this, &DsQmlContentBrowserHelper::visibleChanged);
        setTransientToMainWindow(m_viewer, this);
    }
    if (m_viewer->isMinimized())
        m_viewer->showNormal();
    else
        m_viewer->show();
    m_viewer->raise();
    m_viewer->activateWindow();
}

void DsQmlContentBrowserHelper::hide()
{
    if (m_viewer)
        m_viewer->hide();
}

void DsQmlContentBrowserHelper::setVisible(bool visible)
{
    if (visible)
        show();
    else
        hide();
}

bool DsQmlContentBrowserHelper::isVisible() const
{
    return m_viewer && m_viewer->isVisible();
}

} // namespace dsqt::bridge

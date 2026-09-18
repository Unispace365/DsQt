#include "dsQmlSettingsViewerHelper.h"

#include "settings/dsSettings.h"
#include "settings/dsSettingsViewerWidget.h"

#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QWindow>

namespace dsqt {

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

DsQmlSettingsViewerHelper::DsQmlSettingsViewerHelper(QObject* parent)
    : QObject(parent)
{}

DsQmlSettingsViewerHelper::~DsQmlSettingsViewerHelper() {
    delete m_viewer;
}

void DsQmlSettingsViewerHelper::show() {
    if (!m_viewer) {
        m_viewer = new SettingsViewerWidget(&Settings::instance());
        connect(m_viewer, &SettingsViewerWidget::visibilityChanged,
                this, &DsQmlSettingsViewerHelper::visibleChanged);
        setTransientToMainWindow(m_viewer, this);
    }
    if (m_viewer->isMinimized())
        m_viewer->showNormal();
    else
        m_viewer->show();
    m_viewer->raise();
    m_viewer->activateWindow();
}

void DsQmlSettingsViewerHelper::hide() {
    if (m_viewer)
        m_viewer->hide();
}

void DsQmlSettingsViewerHelper::setVisible(bool visible) {
    if (visible)
        show();
    else
        hide();
}

bool DsQmlSettingsViewerHelper::isVisible() const {
    return m_viewer && m_viewer->isVisible();
}

} // namespace dsqt

#include "bridge/dsQmlContentBrowserHelper.h"
#include "bridge/dsContentBrowserWidget.h"

namespace dsqt::bridge {

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

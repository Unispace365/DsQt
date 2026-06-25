#include "dsQmlSettingsViewerHelper.h"

#include "settings/dsSettings.h"
#include "settings/dsSettingsViewerWidget.h"

namespace dsqt {

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

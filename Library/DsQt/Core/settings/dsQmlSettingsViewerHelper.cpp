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
    if (!m_viewer)
        m_viewer = new SettingsViewerWidget(&Settings::instance());
    m_viewer->show();
    m_viewer->raise();
    m_viewer->activateWindow();
}

} // namespace dsqt

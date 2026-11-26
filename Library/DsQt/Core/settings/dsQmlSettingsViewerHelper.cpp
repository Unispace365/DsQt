#include "dsQmlSettingsViewerHelper.h"

#include "settings/dsSettings.h"
#include "settings/dsSettingsViewerWidget.h"

namespace dsqt {

DsQmlSettingsViewerHelper::DsQmlSettingsViewerHelper(QObject* parent)
    : DsQmlToolWindowHelper(QStringLiteral("settings"), parent)
{}

QWidget* DsQmlSettingsViewerHelper::createWidget() {
    return new SettingsViewerWidget(&Settings::instance());
}

} // namespace dsqt

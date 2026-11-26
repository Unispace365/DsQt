#pragma once

#include "core/dsToolWindow.h"

#include <QQmlEngine>

namespace dsqt {

// QML handle for the Settings window (a SettingsViewerWidget).
//
// Declare it inside DsAppBase (or any Item) and drive it from the menu:
//
//   DsSettingsViewerHelper {
//       id: settingsViewer
//       onVisibleChanged: (isVisible) => { menu.settingsChecked = isVisible }
//   }
//   onSettingsTriggered: (isChecked) => { settingsViewer.setVisible(isChecked) }
//
// show(), hide(), setVisible() and `visible` come from DsQmlToolWindowHelper,
// which also makes the window transient to the main window and remembers its
// geometry (see core/dsToolWindow.h).
class DsQmlSettingsViewerHelper : public DsQmlToolWindowHelper {
    Q_OBJECT
    QML_NAMED_ELEMENT(DsSettingsViewerHelper)

public:
    explicit DsQmlSettingsViewerHelper(QObject* parent = nullptr);

protected:
    QWidget* createWidget() override;
};

} // namespace dsqt

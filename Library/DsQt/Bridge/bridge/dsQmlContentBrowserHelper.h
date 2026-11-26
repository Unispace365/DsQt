#pragma once

#include "core/dsToolWindow.h"

#include <QQmlEngine>

namespace dsqt::bridge {

// QML handle for the Content Browser window (a DsContentBrowserWidget).
//
// Declare it inside DsAppBase (or any Item) and drive it from the menu:
//
//   DsContentBrowserHelper { id: contentBrowser }
//   onContentBrowserTriggered: (isChecked) => { contentBrowser.setVisible(isChecked) }
//
// show(), hide(), setVisible() and `visible` come from DsQmlToolWindowHelper,
// which also makes the window transient to the main window and remembers its
// geometry (see core/dsToolWindow.h).
class DsQmlContentBrowserHelper : public dsqt::DsQmlToolWindowHelper
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DsContentBrowserHelper)

public:
    explicit DsQmlContentBrowserHelper(QObject *parent = nullptr);

protected:
    QWidget *createWidget() override;
};

} // namespace dsqt::bridge

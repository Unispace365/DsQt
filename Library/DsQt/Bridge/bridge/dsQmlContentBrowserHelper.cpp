#include "bridge/dsQmlContentBrowserHelper.h"
#include "bridge/dsContentBrowserWidget.h"

namespace dsqt::bridge {

DsQmlContentBrowserHelper::DsQmlContentBrowserHelper(QObject *parent)
    : DsQmlToolWindowHelper(QStringLiteral("contentBrowser"), parent)
{}

QWidget *DsQmlContentBrowserHelper::createWidget()
{
    return new DsContentBrowserWidget;
}

} // namespace dsqt::bridge

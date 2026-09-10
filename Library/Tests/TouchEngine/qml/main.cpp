#include <QtQuickTest>

#include <QCoreApplication>
#include <QObject>

class TouchEngineQmlTestSetup final : public QObject
{
    Q_OBJECT

public slots:
    void applicationAvailable()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("DesignStudio"));
        QCoreApplication::setOrganizationDomain(QStringLiteral("designstud.io"));
        QCoreApplication::setApplicationName(QStringLiteral("DsqtTouchEngineQmlTests"));
    }
};

QUICK_TEST_MAIN_WITH_SETUP(dsqt_touchengine, TouchEngineQmlTestSetup)

#include "main.moc"

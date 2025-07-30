#ifndef DSRELOADURLINTERCEPTOR_H
#define DSRELOADURLINTERCEPTOR_H

#include <QQmlAbstractUrlInterceptor>
#include <QUrl>
#include <QLoggingCategory>


Q_DECLARE_LOGGING_CATEGORY(lgReloadUrl)
Q_DECLARE_LOGGING_CATEGORY(lgReloadUrlVerbose)
namespace dsqt {
class DsReloadUrlInterceptor : public QQmlAbstractUrlInterceptor
{
public:
    DsReloadUrlInterceptor();
    void setPrefixes(QString proj_from,QString proj_to,QString framework_from="--",QString framework_to="--");

public:
    QUrl intercept(const QUrl &path, QQmlAbstractUrlInterceptor::DataType type) override;
private:
    QString mProjectFromPrefix;
    QString mProjectToPrefix;
    QString mFrameworkFromPrefix;
    QString mFrameworkToPrefix;
};
}//namespace dsqt
#endif // DSRELOADURLINTERCEPTOR_H

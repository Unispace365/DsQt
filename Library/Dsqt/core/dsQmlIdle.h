#ifndef DSQMLIDLE_H
#define DSQMLIDLE_H

#include <QObject>
#include <QQmlEngine>
#include <QTimer>
#include <qloggingcategory.h>

Q_DECLARE_LOGGING_CATEGORY(lgIdle)
Q_DECLARE_LOGGING_CATEGORY(lgIdleVerbose)
namespace dsqt{

class DsQmlIdlePreventer;
class DsQmlIdle : public QObject {
    Q_OBJECT
    QML_ELEMENT
    /// @brief
    Q_PROPERTY(bool idling READ idling NOTIFY idlingChanged FINAL)
    Q_PROPERTY(int idleTimeout READ idleTimeout WRITE setIdleTimeout NOTIFY idleTimeoutChanged FINAL)
    //Q_PROPERTY(DsQmlIdle* parentIdle READ parentIdle WRITE setParentIdle NOTIFY parentIdleChanged FINAL)
    Q_PROPERTY(QQmlListProperty<DsQmlIdlePreventer> idlePreventers READ idlePreventers NOTIFY idlePreventersChanged FINAL)
    Q_PROPERTY(QObject* areaItemTarget READ areaItemTarget WRITE setAreaItemTarget NOTIFY areaItemTargetChanged FINAL)
    friend DsQmlIdlePreventer;

  public:
    explicit DsQmlIdle(QObject* parent=nullptr);


    Q_INVOKABLE void startIdling(bool force=false);
    Q_INVOKABLE void stopIdling(bool force=true);

    QQmlListProperty<DsQmlIdlePreventer> idlePreventers();

    bool idling() const;


    int idleTimeout() const;
    void setIdleTimeout(int newIdleTimeout);

    QObject *areaItemTarget() const;
    void setAreaItemTarget(QObject *newAreaItemTarget);

    void addIdlePreventer(DsQmlIdlePreventer* preventer);
    void clearIdlePreventer(DsQmlIdlePreventer* preventer);
    void clearAllIdlePreventers();

  signals:
    void idleStarted();
    void idleEnded();

    void idlePreventersChanged();

    void idlingChanged();

    void idleTimeoutChanged();

    void areaItemTargetChanged();

  protected:
    bool eventFilter(QObject* watched, QEvent *ev) override;

  private:
    void preventionChanged(bool preventIdle);
    void setIdling(bool newIdling);
    QTimer* mIdleTimer= nullptr;
    QSet<DsQmlIdlePreventer *> mIdlePreventers;
    bool mIdling=true;
    bool mWouldHaveGoneToIdle = false;
    int mIdleTimeout;
    QObject *mAreaItemTarget = nullptr;
};
}//namespace dsqt
#endif // DSQMLIDLE_H

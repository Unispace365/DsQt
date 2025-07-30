#ifndef DSQMLIDLEPREVENTER_H
#define DSQMLIDLEPREVENTER_H

#include <QObject>
#include <QQmlEngine>
#include <Qtimer>

namespace dsqt {


class DsQmlIdle;
class DsQmlIdlePreventer : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(DsIdlePreventer)
    Q_PROPERTY(bool preventIdle READ preventIdle WRITE setPreventIdle NOTIFY preventIdleChanged FINAL)
    Q_PROPERTY(DsQmlIdle* targetIdle READ targetIdle WRITE setTargetIdle NOTIFY targetIdleChanged FINAL)
  public:
    DsQmlIdlePreventer(QObject* parent=nullptr);
    Q_INVOKABLE void preventIdleForMilliseconds(long milliseconds);
    bool preventIdle() const;
    void setPreventIdle(bool newPreventIdle);

    DsQmlIdle *targetIdle() const;
    void setTargetIdle(DsQmlIdle *newTargetIdle);

  signals:
    void preventIdleChanged(bool preventIdle);
    void targetIdleChanged();

  private:
    bool mPreventIdle;
    QTimer* mPreventionTimer;
    DsQmlIdle *mTargetIdle = nullptr;
};
} //namespace dsqt
#endif // DSQMLIDLEPREVENTER_H

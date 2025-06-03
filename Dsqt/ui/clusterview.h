#ifndef CLUSTERVIEW_H
#define CLUSTERVIEW_H

#include "clustermanager.h"
#include "touchcluster.h"

#include <QEvent>
#include <QObject>
#include <QQuickItem>

namespace dsqt::ui {
class ClusterAttachedType:public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(bool minimumMet READ minimumMet WRITE setMinimumMet NOTIFY minimumMetChanged FINAL)
public:
    ClusterAttachedType(QObject *parent);
    bool minimumMet() const;
    void setMinimumMet(bool newMinimumMet);

signals:
    void created();
    void removed();
    void minimumMetChanged();
    void animateOffFinished();
private:
    bool m_minimumMet;
};

class ClusterView : public QQuickItem
{
    Q_OBJECT
    QML_ATTACHED(ClusterAttachedType)
    QML_ELEMENT
    Q_PROPERTY(QQmlComponent* delegate READ delegate WRITE setDelegate NOTIFY delegateChanged FINAL)
    Q_PROPERTY(dsqt::ui::ClusterManager* manager READ manager WRITE setManager NOTIFY managerChanged FINAL)
    //Q_PROPERTY(QQuickItem* target READ target WRITE setTarget NOTIFY targetChanged FINAL)
    Q_CLASSINFO("DefaultProperty", "delegate")
public:
    struct DelegateInstance {
        QQuickItem* mItem;
        bool mInUse = false;
        QMetaObject::Connection mConnection;
    };
    ClusterView(QQuickItem* parent=nullptr);
    QQmlComponent *delegate() const;
    void setDelegate(QQmlComponent *newDelegate);
    ClusterManager *manager() const;
    void setManager(ClusterManager *newManager);
    static ClusterAttachedType* qmlAttachedProperties(QObject* object){
        return new ClusterAttachedType(object);
    }

public slots:
    void onClusterUpdated(const QEventPoint::State& state, dsqt::ui::TouchCluster*);


signals:
    void delegateChanged();
    void managerChanged();

private:
    QQmlComponent *mDelegate = nullptr;
    ClusterManager *mManager = nullptr;
    std::vector<QMetaObject::Connection> mManagerConnections;
    std::unordered_map<int,DelegateInstance*> mInstanceMap;
    std::vector<DelegateInstance*> mInstanceStorage;
    int mMinimumTouchesNeeded = 2;
    void updateInstanceFromCluster(DelegateInstance* instance,TouchCluster* cluster);
};
}//namespace dsqt::ui

#endif // CLUSTERVIEW_H

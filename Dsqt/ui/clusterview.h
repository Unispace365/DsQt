#ifndef CLUSTERVIEW_H
#define CLUSTERVIEW_H

#include "clustermanager.h"
#include "touchcluster.h"

#include <QEvent>
#include <QObject>
#include <QQuickItem>

namespace dsqt::ui {
class ClusterView;
class ClusterAttachedType:public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(bool minimumMet READ minimumMet WRITE setMinimumMet NOTIFY minimumMetChanged FINAL)
public:
    ClusterAttachedType(QObject *parent);
    bool minimumMet() const;
    void setMinimumMet(bool newMinimumMet);

    void setClusterView(ClusterView* view);
    Q_INVOKABLE void closeCluster();

signals:
    void created();
    void removed();
    void released();
    void minimumMetChanged();
    void animateOffFinished();
    void updated(QPointF location);
private:
    bool m_minimumMet;
    ClusterView* m_clusterView;
};

class ClusterView : public QQuickItem
{
    Q_OBJECT
    QML_ATTACHED(ClusterAttachedType)
    QML_ELEMENT
    Q_PROPERTY(QQmlComponent* delegate READ delegate WRITE setDelegate NOTIFY delegateChanged FINAL)
    Q_PROPERTY(dsqt::ui::ClusterManager* manager READ manager WRITE setManager NOTIFY managerChanged FINAL)
    Q_PROPERTY(QVariantList menuModel READ menuModel WRITE setMenuModel NOTIFY menuModelChanged FINAL)
    Q_PROPERTY(QVariantMap menuConfig READ menuConfig WRITE setMenuConfig NOTIFY menuConfigChanged FINAL)
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

    QVariantList menuModel() const;
    void setMenuModel(const QVariantList &newModel);

    QVariantMap menuConfig() const;
    void setMenuConfig(const QVariantMap &newMenuConfig);

public slots:
    void onClusterUpdated(const QEventPoint::State& state, dsqt::ui::TouchCluster*);


signals:
    void delegateChanged();
    void managerChanged();
    void menuAdded(QQuickItem* menu);
    void menuShown(QQuickItem* menu);
    void menuModelChanged();

    void menuConfigChanged();

private:
    QQmlComponent *mDelegate = nullptr;
    ClusterManager *mManager = nullptr;
    std::vector<QMetaObject::Connection> mManagerConnections;
    std::unordered_map<int,DelegateInstance*> mInstanceMap;
    std::vector<DelegateInstance*> mInstanceStorage;
    int mMinimumTouchesNeeded = 2;
    void updateInstanceFromCluster(DelegateInstance* instance,TouchCluster* cluster);
    QVariantList m_menuModel;
    QVariantMap m_menuConfig;
};
}//namespace dsqt::ui

#endif // CLUSTERVIEW_H

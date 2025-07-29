#ifndef DSQMLCLUSTERVIEW_H
#define DSQMLCLUSTERVIEW_H

#include "dsQmlClustermanager.h"
#include "dsQmlTouchcluster.h"

#include <QEvent>
#include <QObject>
#include <QQuickItem>

namespace dsqt::ui {
class DsQmlClusterView;
class DsQmlClusterAttachedType:public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(bool minimumMet READ minimumMet WRITE setMinimumMet NOTIFY minimumMetChanged FINAL)
public:
    DsQmlClusterAttachedType(QObject *parent);
    bool minimumMet() const;
    void setMinimumMet(bool newMinimumMet);

    void setClusterView(DsQmlClusterView* view);
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
    DsQmlClusterView* m_clusterView;
};

class DsQmlClusterView : public QQuickItem
{
    Q_OBJECT
    QML_ATTACHED(DsQmlClusterAttachedType)
    QML_NAMED_ELEMENT(DsClusterView)
    Q_PROPERTY(QQmlComponent* delegate READ delegate WRITE setDelegate NOTIFY delegateChanged FINAL)
    Q_PROPERTY(dsqt::ui::DsQmlClusterManager* manager READ manager WRITE setManager NOTIFY managerChanged FINAL)
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
    DsQmlClusterView(QQuickItem* parent=nullptr);
    QQmlComponent *delegate() const;
    void setDelegate(QQmlComponent *newDelegate);
    DsQmlClusterManager *manager() const;
    void setManager(DsQmlClusterManager *newManager);
    static DsQmlClusterAttachedType* qmlAttachedProperties(QObject* object){
        return new DsQmlClusterAttachedType(object);
    }

    QVariantList menuModel() const;
    void setMenuModel(const QVariantList &newModel);

    QVariantMap menuConfig() const;
    void setMenuConfig(const QVariantMap &newMenuConfig);
protected:
    void componentComplete() override;

public slots:
    void onClusterUpdated(const QEventPoint::State& state, dsqt::ui::DsQmlTouchCluster*);


signals:
    void delegateChanged();
    void managerChanged();
    void menuAdded(QQuickItem* menu);
    void menuShown(QQuickItem* menu);
    void menuModelChanged();

    void menuConfigChanged();

private:
    QQmlComponent *mDelegate = nullptr;
    DsQmlClusterManager *mManager = nullptr;
    std::vector<QMetaObject::Connection> mManagerConnections;
    std::unordered_map<int,DelegateInstance*> mInstanceMap;
    std::vector<DelegateInstance*> mInstanceStorage;
    int mMinimumTouchesNeeded = 2;
    void updateInstanceFromCluster(DelegateInstance* instance,DsQmlTouchCluster* cluster);
    QVariantList m_menuModel;
    QVariantMap m_menuConfig;
};
}//namespace dsqt::ui

#endif // DSQMLCLUSTERVIEW_H

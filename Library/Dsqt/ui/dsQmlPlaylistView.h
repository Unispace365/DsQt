#ifndef DSQMLPLAYLISTVIEW_H
#define DSQMLPLAYLISTVIEW_H

#include <QQuickItem>
#include <QQmlComponent>
namespace dsqt::ui {
class DsQmlPlaylistView : public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DsPlaylistView)
    Q_PROPERTY(QString userKey READ userKey WRITE setUserKey NOTIFY userKeyChanged)
    //when the model changes if there is a template map, we load the corresponding template
    //if there is no templateMap then we log an error but otherwise do nothing.
    Q_PROPERTY(QVariant model READ model WRITE setModel NOTIFY modelChanged)

    Q_PROPERTY(QVariantMap templateMap READ templateMap WRITE setTemplateMap NOTIFY templateMapChanged)
    //interval is the interval a change will happen
    Q_PROPERTY(int interval READ interval WRITE setInterval NOTIFY intervalChanged FINAL)
    Q_PROPERTY(int intervalDefault READ intervalDefault WRITE setIntervalDefault NOTIFY intervalDefaultChanged FINAL)
    Q_PROPERTY(int NoInterval READ noInterval CONSTANT)

public:
    DsQmlPlaylistView();




    int noInterval() const { return INT_MIN; }
    
    
    int intervalDefault() const;
    void setIntervalDefault(int newIntervalDefault);

    int interval() const;
    void setInterval(int newInterval);

    QString userKey() const;
    void setUserKey(const QString &newUserKey);

    QVariant model() const;
    void setModel(const QVariant &newModel);

    QVariantMap templateMap() const;
    void setTemplateMap(const QVariantMap &newTemplateMap);

signals:
    void userKeyChanged();
    void modelChanged();
    void templateMapChanged();
    void templateShown(const QString &userKey, const QVariant &modelItem);
    void templateError(const QString &userKey, const QVariant &modelItem, const QVariantMap &templateMap);
    void navigatedNext();
    void navigatedPrev();

    void intervalDefaultChanged();
    void intervalChanged();

public slots:
    void next();
    void prev();


private:
    QString mUserKey;
    QVariant mModel;
    QVariantMap mTemplateMap;
    int mIntervalDefault;
    int mNoInterval;
    int mInterval;

    QMap<QString,QQmlComponent*> mTemplateComponents;


private:
    void processTemplateMap();
    void loadTemplate();

};
}//namespace dsqt::ui

#endif // DSQMLPLAYLISTVIEW_H

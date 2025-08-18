#ifndef DSQMLPLAYLISTCONTROL_H
#define DSQMLPLAYLISTCONTROL_H

#include <QQuickItem>
#include <QQmlComponent>
#include "model/dsQmlContentModel.h"
#include "core/dsQmlApplicationEngine.h"
namespace dsqt::ui {
class DsQmlPlaylistControl : public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DsPlaylistControl)
    Q_PROPERTY(QString userKey READ userKey WRITE setUserKey NOTIFY userKeyChanged)

    //when the model changes if there is a template map, we load the corresponding template
    //if there is no templateMap then we log an error but otherwise do nothing.
    //TODO rewrite this comment to be more clear
    Q_PROPERTY(dsqt::model::DsQmlContentModel* model READ model WRITE setModel NOTIFY modelChanged)

    // when the model changes: If the mode is NextInterval, we will set the nextModel to the new model and wait
    // for the next call to next() to set the model or prev() to set the model. If the mode is Triggered, we
    // wait for a call to updateModel() to set the model from nextModel. If nextModel is nullptr, we will
    // not set the model.
    Q_PROPERTY(dsqt::model::DsQmlContentModel* nextModel READ nextModel NOTIFY nextModelChanged FINAL)
    Q_PROPERTY(QVariantMap templateMap READ templateMap WRITE setTemplateMap NOTIFY templateMapChanged)
    //interval is the interval between auto advances
    Q_PROPERTY(int interval READ interval WRITE setInterval NOTIFY intervalChanged FINAL)
    Q_PROPERTY(QString intervalKey READ intervalKey WRITE setIntervalKey NOTIFY intervalKeyChanged FINAL)
    Q_PROPERTY(int intervalDefault READ intervalDefault WRITE setIntervalDefault NOTIFY intervalDefaultChanged FINAL)
    Q_PROPERTY(QQmlComponent* currentTemplateComp READ currentTemplateComp  NOTIFY currentItemChanged FINAL)
    Q_PROPERTY(dsqt::model::DsQmlContentModel* currentModelItem READ currentModelItem NOTIFY currentItemChanged FINAL)
    Q_PROPERTY(int NoInterval READ noInterval CONSTANT)
    Q_PROPERTY(dsqt::ui::DsQmlPlaylistControl::UpdateMode updateMode READ updateMode WRITE setUpdateMode NOTIFY updateModeChanged FINAL)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged FINAL)
    Q_PROPERTY(int playlistIndex READ playlistIndex WRITE setPlaylistIndex NOTIFY playlistIndexChanged FINAL)


public:
    DsQmlPlaylistControl();
    enum class UpdateMode {
        Immediate,
        NextInterval,
        Triggered
    };
    Q_ENUM(UpdateMode);

    int noInterval() const { return INT_MIN; }
    
    int intervalDefault() const;
    void setIntervalDefault(int newIntervalDefault);

    int interval() const;
    void setInterval(int newInterval);

    QString userKey() const;
    void setUserKey(const QString &newUserKey);

    dsqt::model::DsQmlContentModel* model() const;
    void setModel(dsqt::model::DsQmlContentModel* newModel);

    QVariantMap templateMap() const;
    void setTemplateMap(const QVariantMap &newTemplateMap);

    QString intervalKey() const;
    void setIntervalKey(const QString &newIntervalKey);

    QQmlComponent *currentTemplateComp() const;

    dsqt::model::DsQmlContentModel *currentModelItem() const;

    dsqt::ui::DsQmlPlaylistControl::UpdateMode updateMode() const;
    void setUpdateMode(const dsqt::ui::DsQmlPlaylistControl::UpdateMode &newUpdateMode);
    Q_INVOKABLE void updateModel();

    dsqt::model::DsQmlContentModel *nextModel() const;

    bool active() const;
    void setActive(bool newActive);

    int playlistIndex() const;
    void setPlaylistIndex(int newPlaylistIndex);

  signals:
    void userKeyChanged();
    void modelChanged();
    void templateMapChanged();
    void modelItemSet(const QString &userKey, const dsqt::model::DsQmlContentModel *modelItem);
    void templateError(const QString &userKey, const dsqt::model::DsQmlContentModel *modelItem, const QVariantMap &templateMap);
    void navigatedNext();
    void navigatedPrev();

    void intervalDefaultChanged();
    void intervalChanged();

    void intervalKeyChanged();

    void currentItemChanged();



    void updateModeChanged();

    void nextModelChanged();

    void activeChanged();

    void playlistIndexChanged();

  public slots:
    void next();
    void prev();


private:
    QString mUserKey;
    dsqt::model::DsQmlContentModel* mModel=nullptr;
    dsqt::model::DsQmlContentModel* mNextModel=nullptr;
    QVariantMap mTemplateMap;
    int mIntervalDefault=30000;
    int mNoInterval = INT_MAX;
    int mInterval=0;
    int m_currentInterval;
    int m_playlistIndex=-1;
    dsqt::DsQmlApplicationEngine* mEngine;

    QMap<QString,QQmlComponent*> mTemplateComponents;

    QTimer *mAdvancingTimer = nullptr;


private:
    void processTemplateMap();
    void handleModelOrTemplateChange();
    bool loadTemplate(int index);

    QString m_intervalKey;
    QQmlComponent *m_currentTemplateComp = nullptr;
    dsqt::model::DsQmlContentModel *m_currentModelItem = nullptr;
    dsqt::ui::DsQmlPlaylistControl::UpdateMode m_updateMode = dsqt::ui::DsQmlPlaylistControl::UpdateMode::Immediate;
    bool m_active;
};
}//namespace dsqt::ui

#endif // DSQMLPLAYLISTCONTROL_H

#ifndef DSQMLPLAYLISTCONTROL_H
#define DSQMLPLAYLISTCONTROL_H

#include "core/dsQmlApplicationEngine.h"
#include "model/dsContentModel.h"
#include <QQmlComponent>
#include <QQuickItem>

namespace dsqt::ui {
class DsQmlPlaylistControl : public QQuickItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(DsPlaylistControl)

    /// @brief User-defined key for identifying the playlist control
    Q_PROPERTY(QString userKey READ userKey WRITE setUserKey NOTIFY userKeyChanged)

    /// @brief The content model for the playlist
    /// @details When the model changes, if a template map exists, the corresponding template is loaded.
    /// If no template map is provided, an error is logged but no further action is taken.
    Q_PROPERTY(dsqt::model::ContentModel* model READ model WRITE setModel NOTIFY modelChanged)

    /// @brief The next content model to be set based on update mode
    /// @details In NextInterval mode, the new model is set as nextModel and applied on the next call to next() or
    /// prev(). In Triggered mode, it waits for updateModel() to set the model. If null, no model change occurs.
    Q_PROPERTY(dsqt::model::ContentModel* nextModel READ nextModel NOTIFY nextModelChanged FINAL)

    /// @brief Map of templates for rendering playlist items
    Q_PROPERTY(QVariantMap templateMap READ templateMap WRITE setTemplateMap NOTIFY templateMapChanged)

    /// @brief Interval (in milliseconds) between automatic playlist advances
    Q_PROPERTY(int interval READ interval WRITE setInterval NOTIFY intervalChanged FINAL)

    /// @brief Key used to retrieve the interval from the model item
    Q_PROPERTY(QString intervalKey READ intervalKey WRITE setIntervalKey NOTIFY intervalKeyChanged FINAL)

    /// @brief Default interval (in milliseconds) when no specific interval is provided
    Q_PROPERTY(int intervalDefault READ intervalDefault WRITE setIntervalDefault NOTIFY intervalDefaultChanged FINAL)

    /// @brief The current template component being used
    Q_PROPERTY(QQmlComponent* currentTemplateComp READ currentTemplateComp NOTIFY currentItemChanged FINAL)

    /// @brief The current model item being displayed
    Q_PROPERTY(dsqt::model::ContentModel* currentModelItem READ currentModelItem NOTIFY currentItemChanged FINAL)

    /// @brief Constant representing no interval
    Q_PROPERTY(int NoInterval READ noInterval CONSTANT)

    /// @brief Update mode for handling model changes
    Q_PROPERTY(dsqt::ui::DsQmlPlaylistControl::UpdateMode updateMode READ updateMode WRITE setUpdateMode NOTIFY
                   updateModeChanged FINAL)

    /// @brief Whether the playlist control is active
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged FINAL)

    /// @brief Current index in the playlist
    Q_PROPERTY(int playlistIndex READ playlistIndex WRITE setPlaylistIndex NOTIFY playlistIndexChanged FINAL)

  public:
    /// @brief Constructor for DsQmlPlaylistControl
    DsQmlPlaylistControl();

    /// @brief Enum for update modes
    enum class UpdateMode {
        Immediate,    ///< Model changes are applied immediately
        NextInterval, ///< Model changes are applied on the next interval
        Triggered     ///< Model changes are applied when updateModel is called
    };
    Q_ENUM(UpdateMode);

    /// @brief Gets the constant representing no interval
    /// @return Integer value INT_MIN
    int noInterval() const { return INT_MIN; }

    /// @brief Gets the default interval for playlist advances
    /// @return The default interval in milliseconds
    int intervalDefault() const;

    /// @brief Sets the default interval for playlist advances
    /// @param newIntervalDefault The new default interval in milliseconds
    void setIntervalDefault(int newIntervalDefault);

    /// @brief Gets the current interval for playlist advances
    /// @return The interval in milliseconds
    int interval() const;

    /// @brief Sets the interval for playlist advances
    /// @param newInterval The new interval in milliseconds
    void setInterval(int newInterval);

    /// @brief Gets the user-defined key for the playlist control
    /// @return The user key as a QString
    QString userKey() const;

    /// @brief Sets the user-defined key for the playlist control
    /// @param newUserKey The new user key
    void setUserKey(const QString& newUserKey);

    /// @brief Gets the current content model
    /// @return Pointer to the DsQmlContentModel
    dsqt::model::ContentModel* model() const;

    /// @brief Sets the content model
    /// @param newModel Pointer to the new DsQmlContentModel
    void setModel(dsqt::model::ContentModel* newModel);

    /// @brief Gets the template map
    /// @return The template map as a QVariantMap
    QVariantMap templateMap() const;

    /// @brief Sets the template map
    /// @param newTemplateMap The new template map
    void setTemplateMap(const QVariantMap& newTemplateMap);

    /// @brief Gets the key used to retrieve the interval from the model item
    /// @return The interval key as a QString
    QString intervalKey() const;

    /// @brief Sets the key used to retrieve the interval from the model item
    /// @param newIntervalKey The new interval key
    void setIntervalKey(const QString& newIntervalKey);

    /// @brief Gets the current template component
    /// @return Pointer to the current QQmlComponent
    QQmlComponent* currentTemplateComp() const;

    /// @brief Gets the current model item
    /// @return Pointer to the current DsQmlContentModel
    dsqt::model::ContentModel* currentModelItem() const;

    /// @brief Gets the current update mode
    /// @return The current UpdateMode
    dsqt::ui::DsQmlPlaylistControl::UpdateMode updateMode() const;

    /// @brief Sets the update mode
    /// @param newUpdateMode The new UpdateMode
    void setUpdateMode(const dsqt::ui::DsQmlPlaylistControl::UpdateMode& newUpdateMode);

    /// @brief Updates the model based on the next model
    /// @param force If true, forces the model update regardless of update mode
    Q_INVOKABLE void updateModel(bool force = false);

    /// @brief Gets the next model to be applied
    /// @return Pointer to the next DsQmlContentModel
    dsqt::model::ContentModel* nextModel() const;

    /// @brief Gets the active state of the playlist control
    /// @return True if the playlist is active, false otherwise
    bool active() const;

    /// @brief Sets the active state of the playlist control
    /// @param newActive The new active state
    void setActive(bool newActive);

    /// @brief Gets the current playlist index
    /// @return The current playlist index
    int playlistIndex() const;

    /// @brief Sets the playlist index
    /// @param newPlaylistIndex The new playlist index
    void setPlaylistIndex(int newPlaylistIndex);

  signals:
    /// @brief Emitted when the user key changes
    void userKeyChanged();

    /// @brief Emitted when the model changes
    void modelChanged();

    /// @brief Emitted when the template map changes
    void templateMapChanged();

    /// @brief Emitted when a model item is set
    /// @param userKey The user-defined key
    /// @param modelItem The model item being set
    void modelItemSet(const QString& userKey, const dsqt::model::ContentModel* modelItem);

    /// @brief Emitted when a template error occurs
    /// @param userKey The user-defined key
    /// @param modelItem The model item with the error
    /// @param templateMap The current template map
    void templateError(const QString& userKey, const dsqt::model::ContentModel* modelItem,
                       const QVariantMap& templateMap);

    /// @brief Emitted when navigating to the next item
    void navigatedNext();

    /// @brief Emitted when navigating to the previous item
    void navigatedPrev();

    /// @brief Emitted when the default interval changes
    void intervalDefaultChanged();

    /// @brief Emitted when the interval changes
    void intervalChanged();

    /// @brief Emitted when the interval key changes
    void intervalKeyChanged();

    /// @brief Emitted when the current item changes
    void currentItemChanged();

    /// @brief Emitted when the update mode changes
    void updateModeChanged();

    /// @brief Emitted when the next model changes
    void nextModelChanged();

    /// @brief Emitted when the active state changes
    void activeChanged();

    /// @brief Emitted when the playlist index changes
    void playlistIndexChanged();

  public slots:
    /// @brief Advances to the next item in the playlist
    void next();

    /// @brief Moves to the previous item in the playlist
    void prev();

  private:
    QString                       mUserKey;
    dsqt::model::ContentModel*    mModel     = nullptr;
    dsqt::model::ContentModel*    mNextModel = nullptr;
    QVariantMap                   mTemplateMap;
    int                           mIntervalDefault = 30000;
    int                           mNoInterval      = INT_MAX;
    int                           mInterval        = 0;
    int                           m_currentInterval;
    int                           m_playlistIndex = -1;
    dsqt::DsQmlApplicationEngine* mEngine;

    QMap<QString, QQmlComponent*> mTemplateComponents;

    QTimer* mAdvancingTimer = nullptr;

    /// @brief Processes the template map to load components
    void processTemplateMap();

    /// @brief Handles changes to the model or template map
    void handleModelOrTemplateChange();

    /// @brief Loads the template for the specified index
    /// @param index The index of the item to load
    /// @return True if the template was loaded successfully, false otherwise
    bool loadTemplate(int index);

    QString                                    m_intervalKey;
    QQmlComponent*                             m_currentTemplateComp = nullptr;
    dsqt::model::ContentModel*                 m_currentModelItem    = nullptr;
    dsqt::ui::DsQmlPlaylistControl::UpdateMode m_updateMode = dsqt::ui::DsQmlPlaylistControl::UpdateMode::Immediate;
    bool                                       m_active;
};
} // namespace dsqt::ui

#endif // DSQMLPLAYLISTCONTROL_H

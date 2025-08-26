#ifndef DSQMLOBJ_H
#define DSQMLOBJ_H

#include "core/dsQmlApplicationEngine.h"
#include "core/dsQmlEnvironment.h"
#include "core/dsQmlPathHelper.h"
#include "model/dsQmlContentModel.h"
#include "model/dsReferenceMap.h"
#include "settings/dsQmlSettingsProxy.h"

#include <QDebug>
#include <QObject>
#include <QQmlEngine>
#include <qqmlintegration.h>

Q_DECLARE_LOGGING_CATEGORY(lgQmlObj)
Q_DECLARE_LOGGING_CATEGORY(lgQmlObjVerbose)
namespace dsqt {

/**
 * \class DsQmlObj
 * \brief Singleton QObject providing QML access to key application components like environment, settings, engine, and
 * platform content.
 *
 * This class acts as a bridge between QML and the underlying C++ application logic. It is registered as a QML singleton
 * named "Ds" and exposes properties and methods for interacting with the application's engine, settings, environment,
 * and content models.
 */
class DsQmlObj : public QObject {

    Q_OBJECT
    QML_SINGLETON
    QML_NAMED_ELEMENT(Ds)

    Q_PROPERTY(dsqt::DsQmlEnvironment* env READ env NOTIFY envChanged)
    Q_PROPERTY(dsqt::DsQmlSettingsProxy* appSettings READ appSettings NOTIFY appSettingsChanged)
    Q_PROPERTY(dsqt::DsQmlApplicationEngine* engine READ engine NOTIFY engineChanged)
    Q_PROPERTY(dsqt::model::DsQmlContentModel* platform READ platform NOTIFY platformChanged)
    Q_PROPERTY(dsqt::DsQmlPathHelper* path READ path CONSTANT)

  public:
    /**
     * \brief Constructs a DsQmlObj instance.
     * \param qmlEngine Pointer to the QQmlEngine instance.
     * \param jsEngine Optional pointer to the QJSEngine instance (defaults to nullptr).
     * \param parent Optional parent QObject (defaults to nullptr).
     */
    explicit DsQmlObj(QQmlEngine* qmlEngine, QJSEngine* jsEngine = nullptr, QObject* parent = nullptr);

    /**
     * \brief Static factory method to create a DsQmlObj instance.
     * \param qmlEngine Pointer to the QQmlEngine instance.
     * \param jsEngine Optional pointer to the QJSEngine instance (defaults to nullptr).
     * \return Pointer to the newly created DsQmlObj instance.
     */
    static DsQmlObj* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine = nullptr);

    /**
     * \brief Getter for the application settings proxy.
     * \return Pointer to DsQmlSettingsProxy, or nullptr if the engine is not available.
     */
    DsQmlSettingsProxy* appSettings() const;

    /**
     * \brief Getter for the QML environment.
     * \return Pointer to DsQmlEnvironment, or nullptr if the engine is not available.
     */
    DsQmlEnvironment* env() const;

    /**
     * \brief Getter for the QML application engine.
     * \return Pointer to DsQmlApplicationEngine, or nullptr if not available.
     */
    DsQmlApplicationEngine* engine() const;

    /**
     * \brief Getter for the platform content model.
     * \return Pointer to DsQmlContentModel representing the platform.
     */
    model::DsQmlContentModel* platform();


    DsQmlPathHelper* path() const;

    /**
     * \brief Retrieves a content model record by its ID.
     * \param id The ID of the record to retrieve.
     * \return Pointer to DsQmlContentModel if found, or nullptr if the ID is empty or not found.
     */
    Q_INVOKABLE model::DsQmlContentModel* getRecordById(QString id) const;

  signals:
    /**
     * \brief Emitted when the environment changes.
     */
    void envChanged();

    /**
     * \brief Emitted when the application settings change.
     */
    void appSettingsChanged();

    /**
     * \brief Emitted when the engine changes.
     */
    void engineChanged();

    /**
     * \brief Emitted when the platform content model changes.
     */
    void platformChanged();

  private:
    DsQmlPathHelper*          mPath        = nullptr;
    DsQmlApplicationEngine*   mEngine      = nullptr;
    model::DsQmlContentModel* mPlatformQml = nullptr;
    model::ContentModelRef    mPlatform;

  private slots:
    /**
     * \brief Slot to update the platform content model.
     */
    void updatePlatform();
};

} // namespace dsqt

#endif // DS_QML_OBJ_H

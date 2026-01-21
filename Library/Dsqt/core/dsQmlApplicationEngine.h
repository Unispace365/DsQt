#ifndef DSQMLAPPLICATIONENGINE_H
#define DSQMLAPPLICATIONENGINE_H

#include "bridge/dsBridgeDatabase.h"
#include "core/dsQmlIdle.h"
#include "core/dsQmlImguiItem.h"
#include "model/dsContentModel.h"
#include "settings/dsSettings.h"

#include <QElapsedTimer>
#include <QFileSystemWatcher>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QTimer>

Q_DECLARE_LOGGING_CATEGORY(lgAppEngine)
Q_DECLARE_LOGGING_CATEGORY(lgAppEngineVerbose)

namespace dsqt {

namespace network {
    class DsNodeWatcher;
}

class DsQmlSettingsProxy;
class DsQmlEnvironment;

/**
 * @class DsQmlApplicationEngine
 * @brief Custom QML application engine for the dsqt framework.
 *
 * This class extends QQmlApplicationEngine to provide additional functionality such as
 * content model management, settings handling, file system watching for hot reloading,
 * and integration with ImGui and node watching. It serves as the core engine for
 * QML-based applications in the dsqt ecosystem.
 *
 * The engine can be initialized, reset, and provides access to application settings,
 * content roots, and other utilities. It also supports QML properties and signals for
 * integration with QML code.
 *
 * @note This class is not creatable from QML; use Ds.engine to access the instance.
 */
class DsQmlApplicationEngine : public QQmlApplicationEngine {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Ya don't need to make an engine. get it from Ds.engine")
    Q_PROPERTY(DsQmlIdle* idle READ idle NOTIFY idleChanged FINAL)
    Q_PROPERTY(model::ContentModel* bridge READ bridge WRITE setBridge NOTIFY bridgeChanged FINAL)

  public:
    /**
     * @brief Constructs a DsQmlApplicationEngine instance.
     * @param parent Optional parent QObject.
     */
    explicit DsQmlApplicationEngine(QObject* parent = nullptr);

    /**
     * @brief Initializes the engine.
     *
     * Performs pre-initialization, emits signals, and calls init and postInit.
     */
    void initialize();

    /**
     * @brief Resets the engine.
     *
     * Performs pre-reset, reloads settings, resets systems, and emits signals.
     */
    void doReset();

    /**
     * @brief Retrieves the application settings reference.
     * @return DsSettingsRef containing the app settings.
     */
    DsSettingsRef getAppSettings();

    /**
     * @brief Sets this engine as the default engine.
     * @param engine Pointer to the DsQmlApplicationEngine to set as default.
     */
    void setDefaultEngine(DsQmlApplicationEngine* engine);

    /**
     * @brief Gets the default engine instance.
     * @return Pointer to the default DsQmlApplicationEngine.
     */
    static DsQmlApplicationEngine* DefEngine();

    /**
     * @brief Gets the ImGui item for QML integration.
     * @return Pointer to DsQmlImguiItem.
     */
    DsQmlImguiItem* imgui();

    /**
     * @brief Clears the QML component cache.
     *
     * Invokable from QML.
     */
    Q_INVOKABLE void clearQmlCache();

    /**
     * @brief Gets the QML environment.
     * @return Pointer to DsQmlEnvironment.
     */
    DsQmlEnvironment* getEnvQml() const;

    /**
     * @brief Gets the application settings proxy.
     * @return Pointer to DsQmlSettingsProxy.
     */
    DsQmlSettingsProxy* getAppSettingsProxy() const;

    /**
     * @brief Gets the node watcher.
     * @return Pointer to network::DsNodeWatcher.
     */
    network::DsNodeWatcher* getNodeWatcher() const;

    /**
     * @brief Reads the settings.
     * @param reset If true, resets the settings before reading.
     */
    void readSettings(bool reset = false);

    /**
     * @brief Gets the idle manager.
     * @return Pointer to DsQmlIdle.
     */
    DsQmlIdle* idle() const;

    /**
     * @brief Returns the Bridge database content. NOT thread-safe! Call from the main thread only!
     * @see database()
     * @return Pointer to model::ContentModel.
     */
    model::ContentModel* bridge() const;

    /**
     * @brief setBridge
     * @param bridge
     */
    void setBridge(model::ContentModel* bridge);

    /**
     * @brief Returns the Bridge database content in a thread-safe manner.
     * @return Const reference to bridge::DatabaseContent.
     */
    const bridge::DatabaseContent& database() const { return mDatabase; }

    /**
     * @brief setDatabase
     * @param database
     */
    void setDatabase(bridge::DatabaseContent&& database) {
        mDatabase = std::move(database);
        emit databaseChanged();
    }

  private:
    /**
     * @brief Performs pre-initialization tasks.
     *
     * Virtual method for subclasses to override.
     */
    virtual void preInit();

    /**
     * @brief Performs initialization tasks.
     *
     * Virtual method for subclasses to override.
     */
    virtual void init();

    /**
     * @brief Performs post-initialization tasks.
     *
     * Virtual method for subclasses to override.
     */
    virtual void postInit();

    /**
     * @brief Resets the idle manager.
     */
    void resetIdle();

    /**
     * @brief Performs pre-reset tasks.
     *
     * Virtual method for subclasses to override.
     */
    virtual void preReset();

    /**
     * @brief Resets the system.
     *
     * Virtual method for subclasses to override.
     */
    virtual void resetSystem();

    /**
     * @brief Performs post-reset tasks.
     *
     * Virtual method for subclasses to override.
     */
    virtual void postReset();

    /**
     * @brief Adds paths recursively to the file watcher.
     * @param path The base path to add.
     * @param recurse If true, adds subdirectories recursively.
     */
    void addRecursive(const QString& path, bool recurse = true);

    /// Application settings reference.
    DsSettingsRef mSettings;

  signals:
    /**
     * @brief Emitted before initialization starts.
     */
    void beginInitialize();

    /**
     * @brief Emitted after initialization completes.
     */
    void endInitialize();

    /**
     * @brief Emitted before reset starts.
     */
    void beginReset();

    /**
     * @brief Emitted after reset completes.
     */
    void endReset();

    /**
     * @brief Emitted when a watched file changes.
     * @param path The path of the changed file.
     */
    void fileChanged(const QString& path);

    /**
     * @brief Emitted when the idle manager changes.
     */
    void idleChanged();

    /**
     * @brief Emitted when the root content is updated. You should only listen to this on the main thread.
     */
    void bridgeChanged();

    /**
     * @brief Emitted when the root content is updated. Thread-safe version.
     */
    void databaseChanged();

  protected:
    /// Static pointer to the default engine instance.
    static DsQmlApplicationEngine* sDefaultEngine;

    /// Pointer to the ImGui item.
    DsQmlImguiItem* mImgui;

    /// File system watcher for hot reloading.
    QFileSystemWatcher* mWatcher = nullptr;

    /// Elapsed timer for last update.
    QElapsedTimer mLastUpdate;

    /// Timer for triggering resets.
    QTimer mTrigger;

    /// Pointer to the application settings proxy.
    DsQmlSettingsProxy* mAppProxy = nullptr;

    /// Pointer to the QML environment.
    DsQmlEnvironment* mQmlEnv = nullptr;

    /// Pointer to the node watcher.
    network::DsNodeWatcher* mNodeWatcher = nullptr;

    /// Pointer to the idle manager.
    DsQmlIdle* mIdle = nullptr;

    /// Point to the content in Bridge CMS.
    mutable model::ContentModel* mBridge = nullptr;

    ///
    bridge::DatabaseContent mDatabase;
};

} // namespace dsqt
#endif // DSQMLAPPLICATIONENGINE_H

#ifndef DSENVIRONMENT_H
#define DSENVIRONMENT_H

#include <QDebug>
#include <qloggingcategory.h>
#include <string>

Q_DECLARE_LOGGING_CATEGORY(lgEnv)
Q_DECLARE_LOGGING_CATEGORY(lgEnvVerbose)
namespace dsqt {

/**
 * @brief Forward declaration of DsSettings class.
 */
class DsSettings;

using DsSettingsRef = std::shared_ptr<DsSettings>;

/**
 * @brief The DsEnvironment class provides access to environment data, such as file paths.
 *
 * This class offers static methods to handle path expansion, contraction, and retrieval of
 * various application and system folders. It is non-instantiable.
 */
class DsEnvironment {
  public:
    DsEnvironment()                     = delete;
    DsEnvironment(const DsEnvironment&) = delete;

    /**
     * @brief Expands the given path by replacing environment variables with their actual values.
     *
     * Supported variables:
     * - %APP% -- expanded to the application folder
     * - %LOCAL% -- expanded to the downstream documents folder
     * - %PP% -- expands to the project path, e.g., "%LOCAL%/cache/%PP%/images/"
     * - %CFG_FOLDER% -- expands to the configuration folder, if it exists
     * - %DOCUMENTS% -- expands to the current user documents folder
     *
     * @param path The path string to expand.
     * @return The expanded path as a std::string.
     */
    static std::string expand(const std::string& path);

    /**
     * @brief Expands the given path by replacing environment variables with their actual values (QString version).
     *
     * Supported variables:
     * - %APP% -- expanded to the application folder
     * - %LOCAL% -- expanded to the downstream documents folder
     * - %PP% -- expands to the project path, e.g., "%LOCAL%/cache/%PP%/images/"
     * - %CFG_FOLDER% -- expands to the configuration folder, if it exists
     * - %DOCUMENTS% -- expands to the current user documents folder
     *
     * @param path The path QString to expand.
     * @return The expanded path as a QString.
     */
    static QString expandq(QString path);

    /**
     * @brief Contracts the given path by inserting applicable environment variables where possible.
     *
     * See expand() for the list of supported variables.
     *
     * @param path The path string to contract.
     * @return The contracted path as a std::string.
     */
    static std::string contract(const std::string& path);

    /**
     * @brief Contracts the given path by inserting applicable environment variables where possible (QString version).
     *
     * See expand() for the list of supported variables.
     *
     * @param path The path QString to contract.
     * @return The contracted path as a QString.
     */
    static QString contract(QString path);

    /**
     * @brief Retrieves the path to a specified app folder, optionally appending a file name.
     *
     * Currently, only "SETTINGS" is valid for folderName. The function searches up the application path
     * to locate the folder, making it suitable for both development and production environments.
     * If fileName is provided, it is appended if the folder exists.
     *
     * @param folderName The name of the application folder to find (e.g., "SETTINGS").
     * @param fileName Optional file name to append to the folder path.
     * @param verify If true, verifies that the folder or file exists; otherwise returns an empty string if not found.
     * @return The path to the application folder (with optional file appended), or empty if not found and verify is
     * true.
     */
    static QString getAppFolder(const QString& folderName, const QString& fileName = {}, bool verify = false);

    /**
     * @brief Retrieves the path to a local settings file or folder.
     *
     * If fileName is empty, returns the local settings folder path.
     *
     * @param fileName The name of the settings file (optional).
     * @return The path to the local settings file or folder.
     */
    static QString getLocalSettingsPath(const QString& fileName);

    /**
     * @brief Loads settings from a file, first attempting the app path, then the local path.
     *
     * Note: Loading settings directly from a DsSettings object does not follow the fallback path.
     *
     * @param settingsName The name of the settings to load.
     * @param fileName The file name to load from.
     * @param lookForOverrides If true, looks for override files.
     * @return A shared pointer to the loaded DsSettings, or nullptr on failure.
     */
    static DsSettingsRef loadSettings(const QString& settingsName, const QString& fileName,
                                      bool lookForOverrides = true);

    /**
     * @brief Loads the engine settings.
     *
     * @return True if settings were loaded successfully, false otherwise.
     */
    static bool loadEngineSettings();

    /**
     * @brief Checks if settings files exist at the appropriate paths.
     *
     * @note This method is not implemented.
     *
     * @param fileName The settings file name to check.
     * @return True if settings exist, false otherwise.
     */
    static bool hasSettings(const QString& fileName);

    /**
     * @brief Saves settings to a local file path.
     *
     * @note This method is not implemented.
     *
     * @param fileName The file name to save to.
     * @param setting The DsSettings object to save.
     */
    static void saveSettings(const QString& fileName, dsqt::DsSettings& setting);

    /**
     * @brief Overrides the expansion behavior for config directory files.
     *
     * @param doOverride True to enable override, false otherwise.
     */
    static void setConfigDirFileExpandOverride(bool doOverride);

    /**
     * @brief Initializes the environment.
     */
    static void initialize();

    /**
     * @brief Retrieves the engine settings.
     *
     * @return A shared pointer to the engine DsSettings.
     */
    static DsSettingsRef engineSettings();

    /**
     * @brief Retrieves the downstream documents folder path.
     *
     * @return The path to the downstream documents folder.
     */
    static QString getDownstreamDocumentsFolder();

  private:
    static QString            sDocuments;
    static QString            sDocumentsDownstream;
    static QString            sAppRootFolder;
    static QString            sProjectPath;
    static QString            sConfigFolder;
    static QString            sSharedFolder;
    static QRegularExpression sEnvRe;
};

} // namespace dsqt

using DsEnv = dsqt::DsEnvironment;

#endif // DSENVIRONMENT_H

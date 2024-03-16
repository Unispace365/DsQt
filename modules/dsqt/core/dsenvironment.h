#ifndef DSENVIRONMENT_H
#define DSENVIRONMENT_H

#include <string>
#include <vector>
#include <QDebug>

namespace dsqt {

class DSSettings;
typedef std::shared_ptr<DSSettings> DSSettingsRef;
/**
 * @brief DSEnvironment
 * Access to the environment data, i.e. file paths etc.
 */
class DSEnvironment {
public:
    DSEnvironment() = delete;
    DSEnvironment(const DSEnvironment&) = delete;

    /// Return the same path but with any environment variables expanded. Current variables:
    ///	%APP% -- expanded to app folder
    ///	%LOCAL% -- expanded to downstream documents folder
    ///  %PP% -- expand the project path, i.e. "%LOCAL%/cache/%PP%/images/"
    ///  %CFG_FOLDER% -- expand to the configuration folder, if it exists
    /// "%DOCUMENTS%" -- expand to current user documents folder
    static std::string			expand(const std::string& path);
	static QString expandq(const QString& path){ return QString::fromStdString(expand(path.toStdString()));}

    /// Return the path but with any applicable environment variables inserted. See expand for variables
    static std::string			contract(const std::string& fullPath);
	static QString contract(const QString& fullPath){
		 return QString::fromStdString(contract(fullPath.toStdString()));
	}
    /// Answer an app folder -- currently only SETTINGS() is valid for arg 1.
    /// If fileName is valid, then it will be appended to the found app folder, if it exists.
    /// This function assumes that I don't actually know the location of the folderName
    /// relative to my appPath, and searches up the appPath looking for it.  This makes
    /// it so no configuration is needed between dev and production environments.
    /// If verify is true, then verify that the folder or file exists, otherwise answer a blank string.
    static std::string			getAppFolder(	const std::string& folderName, const std::string& fileName = "",
                                                const bool verify = false);

    /// Answer a complete path to a local settings file.  Supply an empty file name
    /// to just get the local settings folder.
    static std::string			getLocalSettingsPath(const std::string& fileName);

    /// Convenience to load in a settings file, first from the app path, then the local path
	static DSSettingsRef		loadSettings(const std::string& settingsName, const std::string& filename);

    ///load the engine settings;
    static bool loadEngineSettings();

    /// Check if there are settings at the appropriate paths
    static bool					hasSettings(const std::string& filename);

    /// Convenience to save a settings file to the local path
    static void					saveSettings(const std::string& filename, dsqt::DSSettings& setting);


	static void					setConfigDirFileExpandOverride(const bool doOverride);
	static bool					initialize();
	static DSSettingsRef		engineSettings();
	static std::string			getDownstreamDocumentsFolder();

  private:
	static std::string sDocuments;
    static std::string sDocumentsDownstream;
    static std::string sAppRootFolder;
    static std::string sProjectPath;
    static std::string sConfigFolder;
	static std::string sSharedFolder;

    static bool sInitialized;
};

}//namepace dsqt
using DSEnv = dsqt::DSEnvironment;
#endif // DSENVIRONMENT_H

#include "dsEnvironment.h"
#include <functional>
#include <QStandardPaths>
#include <QDir>
#include <QString>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <qapplicationstatic.h>
#include "settings/dssettings.h"

Q_LOGGING_CATEGORY(lgEnv, "core.environment");
Q_LOGGING_CATEGORY(lgEnvVerbose, "core.environment.verbose");
/// \defgroup QML QML Elements
namespace dsqt {
using namespace Qt::Literals::StringLiterals;
bool DsEnvironment::sInitialized = false;
std::string DsEnvironment::sDocuments = "%DOCUMENTS%";
std::string DsEnvironment::sDocumentsDownstream = "%LOCAL%";
std::string DsEnvironment::sProjectPath = "%PP%";
std::string DsEnvironment::sAppRootFolder="";
std::string DsEnvironment::sConfigFolder = "%CFG_FOLDER%";
std::string DsEnvironment::sSharedFolder = "%SHARED%";
QRegularExpression DsEnvironment::sEnvRe = QRegularExpression(R"((.*?)(\%ENV\%)\((.*?)\)(.*))");


std::string DsEnvironment::expand(const std::string &_path)
{
    if (!sInitialized) DsEnvironment::initialize();

        QString	p = QString::fromStdString(_path);

//        if(USE_CFG_FILE_OVERRIDE && p.find("%APP%") != std::string::npos){
//            std::string tempP = p;

//            boost::replace_first(tempP, "%APP%", "%APP%/settings/%CFG_FOLDER%");
//            boost::replace_all(tempP, "%APP%", ds::App::envAppDataPath());
//            boost::replace_all(tempP, "%CFG_FOLDER%", EngineSettings::getConfigurationFolder());
//            std::string tempPath = Poco::Path(tempP).toString();
//            if(ds::safeFileExistsCheck(tempPath)){
//                return tempPath;
//            }
//        }


        auto match = sEnvRe.match(p);
        while(match.hasCaptured(2)){
            auto envVarName = match.captured(3).trimmed();
            auto value = qEnvironmentVariable(envVarName.toLocal8Bit());
            if(sEnvRe.match(value).hasCaptured(2)){
                value="";
            }
            p = match.captured(1)+value+match.captured(4);
            match = sEnvRe.match(p);
        }

        p.replace("%APP%", QString::fromStdString(sAppRootFolder));
        p.replace("%PP%",QString::fromStdString(sProjectPath));//        boost::replace_all(p, "%PP%", DSEngineSettings::envProjectPath());
        p.replace("%LOCAL%",QString::fromStdString(sDocumentsDownstream));//        boost::replace_all(p, "%LOCAL%", getDownstreamDocumentsFolder());
        p.replace("%CFG_FOLDER%",QString::fromStdString(sConfigFolder));//        boost::replace_all(p, "%CFG_FOLDER%", EngineSettings::getConfigurationFolder());
        p.replace("%DOCUMENTS%",QString::fromStdString(sDocuments));//        boost::replace_all(p, "%DOCUMENTS%", DOCUMENTS);
		p.replace("%SHARE%",QString::fromStdString(sSharedFolder));
		//        // This can result in double path separators, so flatten
//        return Poco::Path(p).toString();
        p = QDir::cleanPath(p);
        return p.toStdString();
}

std::string DsEnvironment::contract(const std::string& _path) {
    if (!sInitialized) DsEnvironment::initialize();

	QString p = QString::fromStdString(_path);

	// boost::replace_all(p, ds::App::envAppDataPath(), "%APP%");
	// boost::replace_all(p, EngineSettings::envProjectPath(), "%PP%");
	// boost::replace_all(p, getDownstreamDocumentsFolder(), "%LOCAL%");
	// boost::replace_all(p, EngineSettings::getConfigurationFolder(), "%CFG_FOLDER%");
	// boost::replace_all(p, DOCUMENTS, "%DOCUMENTS%");
	// // This can result in double path separators, so flatten
	// return Poco::Path(p).toString();

	p.replace(QString::fromStdString(sAppRootFolder), "%APP%");
	p.replace(QString::fromStdString(sProjectPath),
			  "%PP%");	//        boost::replace_all(p, "%PP%", DSEngineSettings::envProjectPath());
	p.replace(QString::fromStdString(sDocumentsDownstream),
			  "%LOCAL%");  //        boost::replace_all(p, "%LOCAL%", getDownstreamDocumentsFolder());
	p.replace(QString::fromStdString(sConfigFolder),
			  "%CFG_FOLDER%");	//        boost::replace_all(p, "%CFG_FOLDER%", EngineSettings::getConfigurationFolder());
	p.replace(QString::fromStdString(sDocuments), "%DOCUMENTS%");  //        boost::replace_all(p, "%DOCUMENTS%", DOCUMENTS);
	p.replace(QString::fromStdString(sSharedFolder), "%SHARE%");
	//        // This can result in double path separators, so flatten
	//        return Poco::Path(p).toString();
	p = QDir::cleanPath(p);
	return p.toStdString();
}

bool DsEnvironment::loadEngineSettings(){
	auto baseConfigFile = expand("%APP%/settings/configuration.toml");
	auto docConfigFile = expand("%LOCAL%/settings/%PP%/configuration.toml");
    auto [found,setting] = DsSettings::getSettingsOrCreate("engine",nullptr);
	auto loaded = setting->loadSettingFile(expand("%APP%/settings/engine.toml"));
	if(loaded){
        std::optional<std::string> projectPath = setting->get<std::string>(std::string("engine.project_path"));
        if(projectPath.has_value()){
            sProjectPath = projectPath.value();
			qCDebug(lgEnv) << "Project Path:" << sProjectPath.c_str();
		}

        auto [cfgFound,config] = DsSettings::getSettingsOrCreate("config",nullptr);
        config->loadSettingFile(baseConfigFile);
        config->loadSettingFile(docConfigFile);

        auto cfgFolder = config->get<std::string>("config_folder");
        if(cfgFolder.has_value()){
            sConfigFolder = cfgFolder.value();
			qCDebug(lgEnv) << "Config Folder:" << sConfigFolder.c_str();
		}
        loadSettings("engine","engine.toml");
        return true;
    }
    return false;
}

DSSettingsRef DsEnvironment::engineSettings(){
    auto engineSettingsRef = DsSettings::getSettings("engine");
	if(!engineSettingsRef) {
		loadEngineSettings();
	}
    return DsSettings::getSettings("engine");
}

DSSettingsRef DsEnvironment::loadSettings(const std::string &settingsName, const std::string &filename,const bool lookForOverrides)
{
    std::string name = settingsName;
    std::string filepath = "%APP%/settings/"+filename;
    std::string cfgFilepath = "%APP%/settings/%CFG_FOLDER%/"+filename;
    std::string docFilepath = "%LOCAL%/settings/%PP%/"+filename;
    std::string cfgDocFilepath = "%LOCAL%/settings/%PP%/%CFG_FOLDER%/"+filename;
    auto [found,setting] = DsSettings::getSettingsOrCreate(name,nullptr);
    auto loaded = setting->loadSettingFile(expand(filepath));
    if(loaded && lookForOverrides){
        setting->loadSettingFile(expand(cfgFilepath));
        setting->loadSettingFile(expand(docFilepath));
        setting->loadSettingFile(expand(cfgDocFilepath));

    }
	return setting;
}

bool DsEnvironment::initialize(){
    if (sInitialized)
            return true;
        sInitialized = true;
        //documents
        std::string documentPath = QStandardPaths::locate(QStandardPaths::HomeLocation,"Documents",QStandardPaths::LocateDirectory).toStdString();
        sDocuments = documentPath;

        //documents/downstream
        auto p = std::filesystem::path(documentPath);
        p.append("downstream");
        sDocumentsDownstream = p.string();

		//programData/Downstream ifAvailable
		QStringList dataPaths = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
		{
			bool found = false;
			QString sharedPath;
			for(auto path:dataPaths){
				if(path.contains("ProgramData")){
					sharedPath = path;
					found = true;
					break;
				}
			}
			if(!found){
				sharedPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
			}
			sharedPath = QDir::cleanPath(sharedPath+"/Downstream");
			sSharedFolder = sharedPath.toStdString();

		}

		//app folder
        auto subParent = [](const std::string& sub, const std::filesystem::path& parent){
            auto p = std::filesystem::path(parent);
            //auto f = p.append(sub);
            if(std::filesystem::exists(p.append(sub)) && std::filesystem::is_directory(p)){
                return p.parent_path().string();
            } else return std::string("");
        };

        auto app_folder_from =[subParent](const std::filesystem::path& checkPath){
            std::string dir(subParent("data",checkPath));
            if(!dir.empty()){
                return dir;
            }
            return subParent("settings",checkPath);
        };

        std::filesystem::path checkPath(qApp->applicationDirPath().toStdString());
        std::string appDataPath = "";
        int count = 0;
        while ((appDataPath=app_folder_from(checkPath)).empty()){
            checkPath = checkPath.parent_path();
            if(count++ >=5 || !checkPath.has_parent_path()) break;
        }
        sAppRootFolder = appDataPath;

        return true;
}

std::string DsEnvironment::getDownstreamDocumentsFolder()
{
    //if (!sInitialized) ds::Environment::initialize();
    //return DOWNSTREAM_DOCUMENTS;
    return "";
}

}

#include "core/dsEnvironment.h"
#include "settings/dsSettings.h"

#include <QDir>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QStandardPaths>
#include <QString>
#include <qapplicationstatic.h>
#include <mutex>

Q_LOGGING_CATEGORY(lgEnv, "core.environment");
Q_LOGGING_CATEGORY(lgEnvVerbose, "core.environment.verbose");
/// \defgroup QML QML Elements
namespace dsqt {

using namespace Qt::Literals::StringLiterals;

QString            DsEnvironment::sDocuments           = "%DOCUMENTS%";
QString            DsEnvironment::sDocumentsDownstream = "%LOCAL%";
QString            DsEnvironment::sProjectPath         = "%PP%";
QString            DsEnvironment::sAppRootFolder       = "";
QString            DsEnvironment::sConfigFolder        = "%CFG_FOLDER%";
QString            DsEnvironment::sSharedFolder        = "%SHARED%";
QRegularExpression DsEnvironment::sEnvRe               = QRegularExpression(R"((.*?)(\%ENV\%)\((.*?)\)(.*))");


std::string DsEnvironment::expand(const std::string& path) {
    return expandq(QString::fromStdString(path)).toStdString();
}

QString DsEnvironment::expandq(QString path) {
    DsEnvironment::initialize();

    auto match = sEnvRe.match(path);
    while (match.hasCaptured(2)) {
        auto variable = match.captured(3).trimmed();
        auto value    = qEnvironmentVariable(variable.toLocal8Bit());
        if (sEnvRe.match(value).hasCaptured(2)) value.clear();
        //        if(USE_CFG_FILE_OVERRIDE && p.find("%APP%") != std::string::npos){
        path  = match.captured(1) + value + match.captured(4);
        match = sEnvRe.match(path);
    }
    //            std::string tempP = p;
    return contract(path);
}

std::string DsEnvironment::contract(const std::string& path) {
    return contract(QString::fromStdString(path)).toStdString();
}

QString DsEnvironment::contract(QString path) {
    DsEnvironment::initialize();
    //        // This can result in double path separators, so flatten
    path.replace("%APP%", sAppRootFolder);
    path.replace("%PP%", sProjectPath);
    path.replace("%LOCAL%", sDocumentsDownstream);
    path.replace("%CFG_FOLDER%", sConfigFolder);
    path.replace("%DOCUMENTS%", sDocuments);
    path.replace("%SHARE%", sSharedFolder);
    //        return Poco::Path(p).toString();
    return QDir::cleanPath(path);
}

bool DsEnvironment::loadEngineSettings() {

    auto [settingsFound, settings] = DsSettings::getSettingsOrCreate("engine", nullptr);

    // boost::replace_all(p, ds::App::envAppDataPath(), "%APP%");
    auto loaded = settings->loadSettingFile(expand("%APP%/settings/engine.toml"));
    if (loaded) {
        // boost::replace_all(p, EngineSettings::envProjectPath(), "%PP%");
        std::optional<std::string> projectPath = settings->get<std::string>(std::string("engine.project_path"));
        if (projectPath.has_value()) {
            sProjectPath = QString::fromStdString(projectPath.value());
            qCDebug(lgEnv) << "Project Path:" << sProjectPath;
        }
        // boost::replace_all(p, getDownstreamDocumentsFolder(), "%LOCAL%");
        // boost::replace_all(p, EngineSettings::getConfigurationFolder(), "%CFG_FOLDER%");
        auto [configFound, config] = DsSettings::getSettingsOrCreate("config", nullptr);
        config->loadSettingFile(expand("%APP%/settings/configuration.toml"));
        // boost::replace_all(p, DOCUMENTS, "%DOCUMENTS%");
        // // This can result in double path separators, so flatten
        config->loadSettingFile(expand("%LOCAL%/settings/%PP%/configuration.toml"));
        // return Poco::Path(p).toString();

        auto cfgFolder = config->get<std::string>("config_folder");
        if (cfgFolder.has_value()) {
            sConfigFolder = QString::fromStdString(cfgFolder.value());
            qCDebug(lgEnv) << "Config Folder:" << sConfigFolder;
        }


        loadSettings("engine", "engine.toml");
        return true;
    }
    return false;
}

DsSettingsRef DsEnvironment::engineSettings() {
    auto engineSettingsRef = DsSettings::getSettings("engine");
    if (!engineSettingsRef) {
        loadEngineSettings();
    }
    return DsSettings::getSettings("engine");
}

DsSettingsRef DsEnvironment::loadSettings(const QString& settingsName, const QString& filename,
                                          const bool lookForOverrides) {
    auto [found, setting] = DsSettings::getSettingsOrCreate(settingsName.toStdString(), nullptr);

    auto loaded = setting->loadSettingFile(expandq("%APP%/settings/" + filename).toStdString());
    if (loaded && lookForOverrides) {
        setting->loadSettingFile(expandq("%APP%/settings/%CFG_FOLDER%/" + filename).toStdString());
        setting->loadSettingFile(expandq("%LOCAL%/settings/%PP%/" + filename).toStdString());
        setting->loadSettingFile(expandq("%LOCAL%/settings/%PP%/%CFG_FOLDER%/" + filename).toStdString());
    }

    return setting;
}

void DsEnvironment::initialize() {
    static std::once_flag initFlag;
    std::call_once(initFlag, []() {
        // documents
        sDocuments = QStandardPaths::locate(QStandardPaths::HomeLocation, "Documents", QStandardPaths::LocateDirectory);

        // documents/downstream
        QDir documents(sDocuments);
        sDocumentsDownstream = QDir::cleanPath(documents.filePath("downstream"));

        // programData/Downstream ifAvailable
        const QStringList dataPaths = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
        {
            QDir sharedPath;
            for (const auto& path : dataPaths) {
                if (path.contains("ProgramData")) {
                    sharedPath = path;
                    break;
                }
            }
            if (!sharedPath.exists()) {
                sharedPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
            }
            sSharedFolder = QDir::cleanPath(sharedPath.filePath("Downstream"));
        }

        // app folder
        QDir currentDir = qApp->applicationDirPath();
        do {
            // auto f = p.append(sub);
            static const QStringList subfolders = {"data", "settings"};
            for (const QString& subfolder : subfolders) {
                QFileInfo subInfo(currentDir.filePath(subfolder));
                if (subInfo.exists() && subInfo.isDir()) {
                    sAppRootFolder = currentDir.absolutePath();
                    break;
                }
            }
        } while (sAppRootFolder.isEmpty() && currentDir.cdUp());
    });
}

QString DsEnvironment::getDownstreamDocumentsFolder() {
    DsEnvironment::initialize();
    return sDocumentsDownstream;
}

} // namespace dsqt

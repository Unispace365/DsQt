#include "core/dsEnvironment.h"
#include "settings/dsSettings.h"

#include <QDir>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QStandardPaths>
#include <QString>
#include <qapplicationstatic.h>

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

        path  = match.captured(1) + value + match.captured(4);
        match = sEnvRe.match(path);
    }

    return contract(path);
}

std::string DsEnvironment::contract(const std::string& path) {
    return contract(QString::fromStdString(path)).toStdString();
}

QString DsEnvironment::contract(QString path) {
    DsEnvironment::initialize();

    path.replace("%APP%", sAppRootFolder);
    path.replace("%PP%", sProjectPath);
    path.replace("%LOCAL%", sDocumentsDownstream);
    path.replace("%CFG_FOLDER%", sConfigFolder);
    path.replace("%DOCUMENTS%", sDocuments);
    path.replace("%SHARE%", sSharedFolder);

    return QDir::cleanPath(path);
}

bool DsEnvironment::loadEngineSettings() {
    auto baseConfigFile   = expand("%APP%/settings/configuration.toml");
    auto docConfigFile    = expand("%LOCAL%/settings/%PP%/configuration.toml");
    auto [found, setting] = DsSettings::getSettingsOrCreate("engine", nullptr);
    auto loaded           = setting->loadSettingFile(expand("%APP%/settings/engine.toml"));
    if (loaded) {
        std::optional<std::string> projectPath = setting->get<std::string>(std::string("engine.project_path"));
        if (projectPath.has_value()) {
            sProjectPath = QString::fromStdString(projectPath.value());
            qCDebug(lgEnv) << "Project Path:" << sProjectPath;
        }

        auto [cfgFound, config] = DsSettings::getSettingsOrCreate("config", nullptr);
        config->loadSettingFile(baseConfigFile);
        config->loadSettingFile(docConfigFile);

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

DSSettingsRef DsEnvironment::engineSettings() {
    auto engineSettingsRef = DsSettings::getSettings("engine");
    if (!engineSettingsRef) {
        loadEngineSettings();
    }
    return DsSettings::getSettings("engine");
}

DSSettingsRef DsEnvironment::loadSettings(const std::string& settingsName, const std::string& filename,
                                          const bool lookForOverrides) {
    std::string name           = settingsName;
    std::string filepath       = "%APP%/settings/" + filename;
    std::string cfgFilepath    = "%APP%/settings/%CFG_FOLDER%/" + filename;
    std::string docFilepath    = "%LOCAL%/settings/%PP%/" + filename;
    std::string cfgDocFilepath = "%LOCAL%/settings/%PP%/%CFG_FOLDER%/" + filename;
    auto [found, setting]      = DsSettings::getSettingsOrCreate(name, nullptr);
    auto loaded                = setting->loadSettingFile(expand(filepath));
    if (loaded && lookForOverrides) {
        setting->loadSettingFile(expand(cfgFilepath));
        setting->loadSettingFile(expand(docFilepath));
        setting->loadSettingFile(expand(cfgDocFilepath));
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

        // programData/Downstream if available
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

        // app folder: search up for folders containing either a "data" or "settings" subfolder.
        QDir currentDir = qApp->applicationDirPath();
        do {
            // Check if any of the subfolders exist.
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

std::string DsEnvironment::getDownstreamDocumentsFolder() {
    // if (!sInitialized) ds::Environment::initialize();
    // return DOWNSTREAM_DOCUMENTS;
    return "";
}

} // namespace dsqt

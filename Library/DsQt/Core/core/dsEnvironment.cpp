#include "core/dsEnvironment.h"
#include "model/dsResource.h"
#include "settings/dsSettings.h"

#include <QDir>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QStandardPaths>
#include <QString>
#include <qapplicationstatic.h>

Q_LOGGING_CATEGORY(lgEnv, "core.environment");
Q_LOGGING_CATEGORY(lgEnvVerbose, "core.environment.verbose");

// Eagerly initialize DsEnvironment on the main thread right after
// QCoreApplication is created, before any worker thread can win the
// std::call_once race in initialize() and end up calling Settings::add()
// on the wrong thread.
static void initDsEnvironment() {
    dsqt::DsEnvironment::initialize();
}
Q_COREAPP_STARTUP_FUNCTION(initDsEnvironment)

namespace dsqt {

using namespace Qt::Literals::StringLiterals;

QString            DsEnvironment::sDocuments           = "%DOCUMENTS%";
QString            DsEnvironment::sDocumentsDownstream = "%LOCAL%";
QString            DsEnvironment::sProjectPath         = "%PP%";
QString            DsEnvironment::sAppRootFolder       = "";
QString            DsEnvironment::sConfigFolder        = "%CFG_FOLDER%";
QString            DsEnvironment::sSharedFolder        = "%SHARED%";
QString            DsEnvironment::sResourceFolder      = "%RES%";
QRegularExpression DsEnvironment::sEnvRe               = QRegularExpression(R"((.*?)(\%ENV\%)\((.*?)\)(.*))");


std::string DsEnvironment::expand(const std::string& path) {
    return expandq(QString::fromStdString(path)).toStdString();
}

QString DsEnvironment::expandq(QString path) {
    auto match = sEnvRe.match(path);
    while (match.hasCaptured(2)) {
        auto variable = match.captured(3).trimmed();
        auto value    = qEnvironmentVariable(variable.toLocal8Bit());
        if (sEnvRe.match(value).hasCaptured(2)) value.clear();

        path  = match.captured(1) + value + match.captured(4);
        match = sEnvRe.match(path);
    }

    path.replace("%APP%", sAppRootFolder);
    path.replace("%LOCAL%", sDocumentsDownstream);
    path.replace("%DOCUMENTS%", sDocuments);
    path.replace("%SHARED%", sSharedFolder);

    path.replace("%PP%", sProjectPath);
    path.replace("%CFG_FOLDER%", sConfigFolder);
    path.replace("%RES%", sResourceFolder);

    return path.contains('%') ? path : QDir::cleanPath(path);
}

QStringList DsEnvironment::expandq(const QStringList& paths) {
    QStringList result;

    for (const QString& raw : paths) {
        const QString expanded = expandq(raw);

        // Drop paths where a variable was not resolved (token still present)
        if (expanded.contains('%')) continue;

        if (!result.contains(expanded)) result.append(expanded);
    }

    return result;
}

std::string DsEnvironment::contract(const std::string& path) {
    return contract(QString::fromStdString(path)).toStdString();
}

QString DsEnvironment::contract(QString path) {
    QString p = path;
    //        // This can result in double path separators, so flatten
    p.replace(sAppRootFolder, "%APP%");
    p.replace(sProjectPath, "%PP%");
    p.replace(sDocumentsDownstream, "%LOCAL%");
    p.replace(sConfigFolder, "%CFG_FOLDER%");
    p.replace(sDocuments, "%DOCUMENTS%");
    p.replace(sSharedFolder, "%SHARED%");

    p = QDir::cleanPath(p);
    return p;
}

bool DsEnvironment::loadEngineSettings() {
    qCInfo(lgEnv) << "\nLoad Settings >>>>>>>>>>>>>>>>>>>>>>>>";

    // Make sure the default settings path is used to find the settings.
    Settings::instance().setSearchPaths({"%APP%/settings"});

    // Make sure the engine settings are loaded.
    qCInfo(lgEnv) << "loading main engine.toml";
    Settings::add("engine");

    // Setup the project path (%PP%).
    const auto projectPath = Settings::find<QString>("engine", "engine.project_path");
    if (!projectPath.isEmpty()) {
        sProjectPath = projectPath;
        qCDebug(lgEnv) << "Project Path:" << sProjectPath;

        // Append to search paths.
        Settings::instance().setSearchPaths({"%APP%/settings", "%LOCAL%/settings/%PP%"});
    }

    // Setup config folder path (%CFG_FOLDER%).
    Settings::add("configuration");

    const auto cfgFolder = Settings::find<QString>("configuration", "config_folder");
    if (!cfgFolder.isEmpty()) {
        sConfigFolder = cfgFolder;
        qCDebug(lgEnv) << "Config Folder:" << sConfigFolder;

        // Append to search paths.
        Settings::instance().setSearchPaths({"%APP%/settings", "%APP%/settings/%CFG_FOLDER%", "%LOCAL%/settings/%PP%",
                                             "%LOCAL%/settings/%PP%/%CFG_FOLDER%"});
    }

    sResourceFolder = Settings::find<QString>("engine", "engine.resource.location");
    if (!sResourceFolder.isEmpty()) {
        if (sResourceFolder.contains("%USERPROFILE%")) {
#ifndef _WIN32
            sResourceFolder.replace("%USERPROFILE%", QDir::homePath());
            qCInfo(lgEnv)
                << "Non-windows workaround: Converting \"%USERPROFILE%\" to \"~\" in resources_location...";
#endif
        }
        sResourceFolder = expandq(sResourceFolder);
        sResourceFolder = QUrl::fromLocalFile(sResourceFolder).toString();

        const auto resourceDb = Settings::find<QString>("engine", "engine.resource.resource_db");
        DsResource::Id::setupPaths(sResourceFolder, QDir::fromNativeSeparators(resourceDb),
                                   QDir::fromNativeSeparators(projectPath));
    }

    qCInfo(lgEnv) << "loading main app_settings.toml";
    Settings::add("app_settings");

    auto *engineSettings = Settings::instance().settingsFile("engine");
    if (engineSettings) {
        const QVariantMap extras = engineSettings->getObj("engine.extra");
        for (auto it = extras.cbegin(); it != extras.cend(); ++it) {
            auto *settingsFile = Settings::instance().settingsFile(it.key());
            if (!settingsFile) {
                qCDebug(lgEnv) << "Ignoring engine.extra entry for unknown settings file" << it.key();
                continue;
            }

            const QStringList extraFiles = it.value().toStringList();
            qCInfo(lgEnv) << "loading extra settings files for" << it.key() << extraFiles;
            settingsFile->setExtraFiles(extraFiles);
        }
    }

    qCInfo(lgEnv) << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n";

    return true;
}

SettingsFile* DsEnvironment::engineSettings() {
    if (!Settings::instance().hasSettingsFile("engine")) loadEngineSettings();
    return Settings::instance().settingsFile("engine");
}

SettingsFile* DsEnvironment::appSettings() {
    if (!Settings::instance().hasSettingsFile("app_settings")) loadEngineSettings();
    return Settings::instance().settingsFile("app_settings");
}

SettingsFile* DsEnvironment::loadSettings(const QString& settingsName, const QString& filename,
                                          const bool lookForOverrides, const bool forceOverrides) {
    Settings::add(settingsName, filename);
    return Settings::instance().settingsFile(settingsName);
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

        // // Open these settings files if available. This will kick off the calls to update().
        // Settings::add("configuration");
        // Settings::add("engine");

        // // Connect to Settings to listen for changes.
        // auto app = QCoreApplication::instance();
        // QObject::connect(&Settings::instance(), &Settings::instancesChanged, app, &DsEnvironment::update);
        // QObject::connect(&Settings::instance(), &Settings::searchPathsChanged, app, &DsEnvironment::update);

        // update();
    });
}

void DsEnvironment::update() {
    // auto pp         = Settings::find<QString>("engine", "engine.project_path", "%PP%");
    // sProjectPath    = Settings::find<QString>("engine", "engine.project_path", "%PP%");
    // auto cf         = Settings::find<QString>("configuration", "config_folder", "%CFG_FOLDER%");
    // sConfigFolder   = Settings::find<QString>("configuration", "config_folder", "%CFG_FOLDER%");
    // auto rf         = expandq(Settings::find<QString>("engine", "engine.resource.location", ""));
    // sResourceFolder = expandq(Settings::find<QString>("engine", "engine.resource.location", ""));

    // // Expand environment variables based on available settings.
    // QStringList paths;
    // paths.append("%APP%/settings");
    // paths.append("%APP%/settings/%CFG_FOLDER%");
    // paths.append("%LOCAL%/settings/%PP%");
    // paths.append("%LOCAL%/settings/%PP%/%CFG_FOLDER%");

    // QStringList expanded = DsEnvironment::expandq(paths);
    // Settings::instance().setSearchPaths(expanded);
}

QString DsEnvironment::getDownstreamDocumentsFolder() {
    return sDocumentsDownstream;
}

} // namespace dsqt

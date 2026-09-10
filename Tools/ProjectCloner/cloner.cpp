#include "cloner.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QThread>
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLibraryInfo>
#include <QMap>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

namespace {

struct VisualStudioInstallation
{
    QString generator;
    QString displayName;
    QString presetPrefix;
    QString installationPath;

    bool isValid() const
    {
        return !generator.isEmpty() && !installationPath.isEmpty();
    }
};

QString normalizedExistingPath(const QString &path)
{
    const QFileInfo info(QDir::cleanPath(path));
    if (!info.exists())
        return {};

    const QString canonicalPath = info.canonicalFilePath();
    return QDir::fromNativeSeparators(canonicalPath.isEmpty() ? info.absoluteFilePath()
                                                               : canonicalPath);
}

QMap<QString, QString> discoverQtInstallations()
{
    static const QRegularExpression versionRe(QStringLiteral(R"(^\d+\.\d+\.\d+$)"));
    static const QRegularExpression msvcKitRe(QStringLiteral(R"(^msvc\d+_64$)"),
                                               QRegularExpression::CaseInsensitiveOption);

    QMap<QString, QString> installations;

    const auto addQtPrefix = [&installations](const QString &path) {
        const QString prefix = normalizedExistingPath(path);
        if (prefix.isEmpty())
            return;

        const QDir prefixDir(prefix);
        if (!QFileInfo::exists(prefixDir.filePath(QStringLiteral("lib/cmake/Qt6/Qt6Config.cmake")))
            && !QFileInfo::exists(prefixDir.filePath(QStringLiteral("bin/qmake.exe")))) {
            return;
        }

        QDir versionDir(prefixDir);
        if (!versionDir.cdUp())
            return;

        const QString version = versionDir.dirName();
        if (!versionRe.match(version).hasMatch())
            return;

        // Prefer the first location found. QLibraryInfo and environment-provided
        // locations are scanned before conventional installation roots.
        if (!installations.contains(version))
            installations.insert(version, prefix);
    };

    const auto scanRoot = [&addQtPrefix](const QString &rootPath) {
        QDir root(rootPath);
        if (!root.exists())
            return;

        // QT_DIR/QTDIR commonly point directly at a compiler-specific Qt prefix.
        addQtPrefix(root.absolutePath());

        QStringList versionDirectories;
        if (versionRe.match(root.dirName()).hasMatch()) {
            versionDirectories.append(root.absolutePath());
        } else {
            const auto entries = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
            for (const auto &entry : entries) {
                if (versionRe.match(entry).hasMatch())
                    versionDirectories.append(root.filePath(entry));
            }
        }

        for (const auto &versionPath : versionDirectories) {
            QDir versionDir(versionPath);
            const auto kitDirectories = versionDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot,
                                                               QDir::Name);
            for (const auto &kitDirectory : kitDirectories) {
                if (msvcKitRe.match(kitDirectory).hasMatch())
                    addQtPrefix(versionDir.filePath(kitDirectory));
            }
        }
    };

    addQtPrefix(QLibraryInfo::path(QLibraryInfo::PrefixPath));

    for (const auto &variable : {"QT_DIR", "QTDIR"}) {
        const QString path = qEnvironmentVariable(variable);
        if (!path.isEmpty())
            scanRoot(path);
    }

    scanRoot(QStringLiteral("C:/Qt"));
    scanRoot(QStringLiteral("/mnt/c/Qt"));

    return installations;
}

VisualStudioInstallation discoverVisualStudioInstallation()
{
    QString vswhere = QStandardPaths::findExecutable(QStringLiteral("vswhere.exe"));
    if (vswhere.isEmpty()) {
        const QStringList candidates = {
            qEnvironmentVariable("ProgramFiles(x86)")
                + QStringLiteral("/Microsoft Visual Studio/Installer/vswhere.exe"),
            qEnvironmentVariable("ProgramFiles")
                + QStringLiteral("/Microsoft Visual Studio/Installer/vswhere.exe")
        };
        for (const auto &candidate : candidates) {
            vswhere = normalizedExistingPath(candidate);
            if (!vswhere.isEmpty())
                break;
        }
    }

    if (vswhere.isEmpty())
        return {};

    QProcess process;
    process.start(vswhere, {
        QStringLiteral("-products"), QStringLiteral("*"),
        QStringLiteral("-requires"),
        QStringLiteral("Microsoft.VisualStudio.Component.VC.Tools.x86.x64"),
        QStringLiteral("-format"), QStringLiteral("json"),
        QStringLiteral("-utf8")
    });
    if (!process.waitForStarted(3000) || !process.waitForFinished(5000))
        return {};

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(process.readAllStandardOutput(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray())
        return {};

    VisualStudioInstallation selected;
    int selectedMajor = 0;
    for (const auto &value : document.array()) {
        const auto instance = value.toObject();
        const QString version = instance.value(QStringLiteral("installationVersion")).toString();
        const int major = version.section('.', 0, 0).toInt();

        QString year;
        if (major == 16)
            year = QStringLiteral("2019");
        else if (major == 17)
            year = QStringLiteral("2022");
        else if (major == 18)
            year = QStringLiteral("2026");
        else
            continue;

        const QString installationPath = normalizedExistingPath(
            instance.value(QStringLiteral("installationPath")).toString());
        if (installationPath.isEmpty() || major <= selectedMajor)
            continue;

        selectedMajor = major;
        selected.generator = QStringLiteral("Visual Studio %1 %2").arg(major).arg(year);
        selected.displayName = QStringLiteral("VS%1").arg(year);
        selected.presetPrefix = QStringLiteral("vs%1").arg(year);
        selected.installationPath = installationPath;
    }

    return selected;
}

QString discoverNinjaExecutable(const VisualStudioInstallation &visualStudio,
                                const QMap<QString, QString> &qtInstallations)
{
    QString ninja = QStandardPaths::findExecutable(QStringLiteral("ninja.exe"));
    if (ninja.isEmpty())
        ninja = QStandardPaths::findExecutable(QStringLiteral("ninja"));
    if (!ninja.isEmpty())
        return normalizedExistingPath(ninja);

    QStringList candidates;
    for (const auto &qtPrefixPath : qtInstallations) {
        QDir qtPrefix(qtPrefixPath);
        candidates.append(qtPrefix.absoluteFilePath(QStringLiteral("../../Tools/Ninja/ninja.exe")));
    }
    candidates.append(QStringLiteral("C:/Qt/Tools/Ninja/ninja.exe"));

    for (const auto &variable : {"QT_DIR", "QTDIR"}) {
        QDir location(qEnvironmentVariable(variable));
        if (location.exists()) {
            candidates.append(location.absoluteFilePath(QStringLiteral("Tools/Ninja/ninja.exe")));
            candidates.append(location.absoluteFilePath(QStringLiteral("../../Tools/Ninja/ninja.exe")));
        }
    }

    if (visualStudio.isValid()) {
        candidates.append(QDir(visualStudio.installationPath).filePath(
            QStringLiteral("Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe")));
    }

    for (const auto &candidate : candidates) {
        ninja = normalizedExistingPath(candidate);
        if (!ninja.isEmpty())
            return ninja;
    }

    return {};
}

} // namespace

Cloner::Cloner() {
    checker = new GitIgnoreChecker(QCoreApplication::applicationDirPath() + "/skip.txt");

    // Resolve ClonerSource location: prefer installed layout relative to exe,
    // fall back to compile-time source tree path for development builds.
    QString installedPath = QCoreApplication::applicationDirPath() + "/../../Examples/ClonerSource";
    if (QDir(installedPath).exists()) {
        m_cloneFromDirectory = QDir(installedPath).canonicalPath();
    } else {
        m_cloneFromDirectory = QStringLiteral(CLONER_SOURCE_DIR);
    }
    detectQtVersions();
}

void Cloner::clone()
{
    if(m_status == Status::PRECLONE){
        QThread* thread = QThread::create([this](){
            //check the destination. It needs to not exist
            //or be empty
            if(m_cloneToDirectory.isEmpty()){
                setLastError("Destination directory was not given");
                emit lastErrorSet();
                setStatus(Status::POSTCLONE);
                return;
            }

            bool destExists = QFileInfo::exists(m_cloneToDirectory);
            if(destExists){
                QDir destDir(m_cloneToDirectory);
                auto inDir = destDir.entryList();
                if(inDir.size()>0){
                    setLastError("Destination directory is not empty");
                    emit lastErrorSet();
                    setStatus(Status::POSTCLONE);
                    return;
                }
            } else {
                QDir dir;
                dir.mkpath(m_cloneToDirectory);
            }

            QDir destDirectory(m_cloneToDirectory);


            //copy the source to the destination, one file at a time
            //checking against the excludeLists
            auto sourceDir = QDir(m_cloneFromDirectory);
            if(!processDirectory(sourceDir,sourceDir)){
                setStatus(Status::POSTCLONE);
                return;
            }

            // Compute parent directory (project-level) from clone destination
            QDir cloneDir(m_cloneToDirectory);
            if (m_usePlatformDir) {
                QDir parentDir(cloneDir.absolutePath());
                parentDir.cdUp();
                m_parentDirectory = parentDir.absolutePath();
            } else {
                m_parentDirectory = cloneDir.absolutePath();
            }

            if(m_copyTopLevel) {
                QString projectSourcePath = QCoreApplication::applicationDirPath() + "/ProjectSource";
                QDir projectSourceDir(projectSourcePath);
                if(projectSourceDir.exists()) {
                    copyIfNotExists(projectSourceDir, projectSourceDir, m_parentDirectory);
                } else {
                    qInfo() << "ProjectSource not found at" << projectSourcePath << ", skipping top-level copy.";
                }

                if (m_usePlatformDir) {
                    addPlatformToProjectReadme(m_parentDirectory);
                }
            }
            QDir parentDirectory(m_parentDirectory);


            //run through the CMakeLists.txt and Readme.md making substitutions as needed.
            //Main.cpp
            QString substitutionName = m_usePlatformDir ? m_platformName : m_projectName;
            bool success = true;
            success = success && replaceInFile(destDirectory.absoluteFilePath("main.cpp"),"ClonerSource",m_applicationName);
            success = success && replaceInFile(destDirectory.absoluteFilePath("Main.qml"),"ClonerSource",m_applicationName);
            success = success && replaceInFile(destDirectory.absoluteFilePath("settings/app_settings.toml"),"ClonerSource",m_applicationName);
            success = success && replaceInFile(destDirectory.absoluteFilePath("settings/engine.toml"),"ClonerSource",m_applicationName);
            success = success && replaceInFile(destDirectory.absoluteFilePath("CMakeLists.txt"),"ClonerSource",m_applicationName);
            success = success && replaceInFile(destDirectory.absoluteFilePath("CMakeLists.txt"),"PROJECT_NAME_",substitutionName);
            success = success && replaceInFile(destDirectory.absoluteFilePath("CMakeLists.txt"),"PROJECT_DESC_",m_applicationName);
            success = success && replaceInFile(destDirectory.absoluteFilePath("README.md"),"PROJECT_NAME_",substitutionName);
            success = success && replaceInFile(destDirectory.absoluteFilePath("README.md"),"APP_NAME_",m_applicationName);
            //update README.md in project dir
            if(m_copyTopLevel) {
                success = success && replaceInFile(parentDirectory.absoluteFilePath("README.md"),"PROJECT_NAME_",m_projectName);
                success = success && replaceInFile(parentDirectory.absoluteFilePath("README.md"),"APP_NAME",m_applicationName);
            }
            //get all the files in the qml/ directory and replace occurrences there too.
            QDir qmlDir(destDirectory.absoluteFilePath("qml"));
            qDebug()<<"Processing QML files in "<<qmlDir.absolutePath();
            auto qmlFiles = qmlDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot | QDir::AllDirs | QDir::AllEntries, QDir::Name);
            for(const auto& fileInfo:qmlFiles){
                success = success && replaceInFile(fileInfo.absoluteFilePath(),"ClonerSource",m_applicationName);
                success = success && replaceInFile(fileInfo.absoluteFilePath(),"PROJECT_NAME_",substitutionName);
                qDebug()<<"Processed "<<fileInfo.absoluteFilePath();
            }

            if(!success) {
                setStatus(Status::POSTCLONE);
                return;
            }

            if (!m_selectedQtVersions.isEmpty()) {
                if (!writeCMakePresets(destDirectory.absolutePath(), m_selectedQtVersions)) {
                    setStatus(Status::POSTCLONE);
                    return;
                }
            }

            qInfo()<<"Done cloning!";
            emit cloned();
            setStatus(Status::POSTCLONE);
        });
        thread->start();
    }
}

void Cloner::clearLastError()
{
    setLastError("");
    emit lastErrorCleared();
}

void Cloner::reset()
{
    clearLastError();
    setStatus(Status::PRECLONE);
}

QString Cloner::applicationName() const
{
    return m_applicationName;
}

void Cloner::setApplicationName(const QString &newApplicationName)
{
    if (m_applicationName == newApplicationName)
        return;
    m_applicationName = newApplicationName;
    emit applicationNameChanged();
}

QString Cloner::projectName() const
{
    return m_projectName;
}

void Cloner::setProjectName(const QString &newProjectName)
{
    if (m_projectName == newProjectName)
        return;
    m_projectName = newProjectName;
    emit projectNameChanged();
}

QString Cloner::platformName() const
{
    return m_platformName;
}

void Cloner::setPlatformName(const QString &newPlatformName)
{
    if (m_platformName == newPlatformName)
        return;
    m_platformName = newPlatformName;
    emit platformNameChanged();
}

QString Cloner::cloneToDirectory() const
{
    return m_cloneToDirectory;
}

void Cloner::setCloneToDirectory(const QString &newCloneToDirectory)
{
    if (m_cloneToDirectory == newCloneToDirectory)
        return;
    m_cloneToDirectory = newCloneToDirectory;
    emit cloneToDirectoryChanged();
}

QString Cloner::cloneFromDirectory() const
{
    return m_cloneFromDirectory;
}

void Cloner::setCloneFromDirectory(const QString &newCloneFromDirectory)
{
    if (m_cloneFromDirectory == newCloneFromDirectory)
        return;
    m_cloneFromDirectory = newCloneFromDirectory;
    emit cloneFromDirectoryChanged();
}

QList<QString> Cloner::excludeList() const
{
    return m_excludeList;
}

void Cloner::setExcludeList(const QList<QString> &newExcludeList)
{
    if (m_excludeList == newExcludeList)
        return;
    m_excludeList = newExcludeList;
    emit excludeListChanged();
}

QString Cloner::lastError() const
{
    return m_lastError;
}

void Cloner::setLastError(const QString &newLastError)
{
    if (m_lastError == newLastError)
        return;
    m_lastError = newLastError;
    emit lastErrorChanged();
}

using namespace Qt::Literals::StringLiterals;
bool Cloner::processDirectory(QDir dir,QDir rootDir)
{

    auto sourceItems = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
    for(const auto& item:static_cast<const QFileInfoList>(sourceItems)){
        const auto name = rootDir.relativeFilePath(item.absoluteFilePath());
        const auto sourceName =  item.absoluteFilePath();
        const auto destName = m_cloneToDirectory+"/"+name;
        if(!checker->isIgnored(name)){


            if(item.isDir()){
                if(rootDir.mkpath(destName)){
                    qInfo()<<"creating directory";
                }
                QDir nextDir(item.filePath());

                qInfo()<<"processing directory "<<name;
                if(!processDirectory(nextDir,rootDir)) {

                    return false;
                }
            } else {
                if(!QFile::copy(sourceName,destName)){
                    setLastError(u"Error copying "_s + sourceName +" to " + destName +". Aborted");
                    emit lastErrorSet();
                    return false;
                }
                qInfo()<<"copying "<<name<<" to "<<destName;
            }

        } else {
            qInfo()<<"skipping "<<name;
        }
    }
    return true;
}

bool Cloner::replaceInFile(QString fileName, QString match, QString replacement)
{

    QByteArray fileData;
    QFile file(fileName);
    if(!file.open(QIODevice::ReadOnly)){
        setLastError("Failed to process "+fileName+". Could not open file.");
        emit lastErrorSet();
        return false;
    }// open for read and write
    fileData = file.readAll(); // read all the data into the byte array
    QString text(fileData); // add to text string for easy string replace
    file.close();


    text.replace(match, replacement); // replace text in string

    if(!file.open(QIODevice::ReadWrite | QIODevice::Truncate)){
        setLastError("Failed to process "+fileName+". Could not open file.");
        emit lastErrorSet();
        return false;
    }
    if(!file.seek(0)){
        setLastError("Failed to process "+fileName+". Could not write file.");
        emit lastErrorSet();
        return false;
    } // go to the beginning of the file
    if(file.write(text.toUtf8())<0) {
        setLastError("Failed to process "+fileName+". Could not write file.");
        emit lastErrorSet();
        return false;
    } // write the new text back to the file

    file.close(); // close the file handle.
    return true;
}

bool Cloner::copyTopLevel() const
{
    return m_copyTopLevel;
}

void Cloner::setCopyTopLevel(bool newCopyTopLevel)
{
    if (m_copyTopLevel == newCopyTopLevel)
        return;
    m_copyTopLevel = newCopyTopLevel;
    emit copyTopLevelChanged();
}

bool Cloner::copyIfNotExists(QDir sourceDir, QDir sourceRoot, const QString &destRoot)
{
    auto entries = sourceDir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
    for (const auto &entry : entries) {
        QString relativePath = sourceRoot.relativeFilePath(entry.absoluteFilePath());
        QString destPath = destRoot + "/" + relativePath;

        if (entry.isDir()) {
            QDir().mkpath(destPath);
            if (!copyIfNotExists(QDir(entry.absoluteFilePath()), sourceRoot, destRoot))
                return false;
        } else {
            if (!QFile::exists(destPath)) {
                QDir().mkpath(QFileInfo(destPath).absolutePath());
                if (!QFile::copy(entry.absoluteFilePath(), destPath)) {
                    qWarning() << "Failed to copy" << entry.absoluteFilePath() << "to" << destPath;
                    return false;
                }
                qInfo() << "Copied top-level file:" << relativePath;
            } else {
                qInfo() << "Skipping existing file:" << relativePath;
            }
        }
    }
    return true;
}

void Cloner::addPlatformToProjectReadme(const QString &projectDir)
{
    QString readmePath = projectDir + "/README.md";
    QFile file(readmePath);
    if (!file.exists()) {
        qInfo() << "Project README.md not found at" << readmePath << ", skipping platform link.";
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Could not open" << readmePath << "for reading.";
        return;
    }
    QString text = QString::fromUtf8(file.readAll());
    file.close();

    const QString startMarker = "[PLATFORMS]:#";
    const QString endMarker = "[END_PLATFORMS]:#";

    int startIdx = text.indexOf(startMarker);
    int endIdx = text.indexOf(endMarker);
    if (startIdx < 0 || endIdx < 0 || endIdx <= startIdx) {
        qInfo() << "Platform markers not found in" << readmePath << ", skipping platform link.";
        return;
    }

    // Extract the block between the markers
    int contentStart = startIdx + startMarker.length();
    QString existingBlock = text.mid(contentStart, endIdx - contentStart);

    // Build the new link line
    QString linkLine = "- [" + m_platformName + "](" + m_applicationName + "/README.md)";

    // Check if this platform link is already present
    if (existingBlock.contains(linkLine)) {
        qInfo() << "Platform link already present in" << readmePath;
        return;
    }

    // Append the new link, preserving existing entries
    // ensureing there is a blank line post link
    QString newBlock = existingBlock.trimmed().isEmpty()
        ? "\n" + linkLine + "\n"
        : existingBlock.trimmed() + "\n" + linkLine + "\n\n";

    QString newText = text.left(contentStart) + "\n" + newBlock + text.mid(endIdx);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "Could not open" << readmePath << "for writing.";
        return;
    }
    file.write(newText.toUtf8());
    file.close();

    qInfo() << "Added platform link for" << m_platformName << "to" << readmePath;
}

Cloner::Status Cloner::status() const
{
    return m_status;
}

void Cloner::setStatus(Status newStatus)
{
    if (m_status == newStatus)
        return;
    m_status = newStatus;
    emit statusChanged();
}

bool Cloner::usePlatformDir() const
{
    return m_usePlatformDir;
}

void Cloner::setUsePlatformDir(bool newUsePlatformDir)
{
    if (m_usePlatformDir == newUsePlatformDir)
        return;
    m_usePlatformDir = newUsePlatformDir;
    emit usePlatformDirChanged();
}

QStringList Cloner::detectedQtVersions() const
{
    return m_detectedQtVersions;
}

QStringList Cloner::selectedQtVersions() const
{
    return m_selectedQtVersions;
}

void Cloner::setSelectedQtVersions(const QStringList &newSelectedQtVersions)
{
    if (m_selectedQtVersions == newSelectedQtVersions)
        return;
    m_selectedQtVersions = newSelectedQtVersions;
    emit selectedQtVersionsChanged();
}

void Cloner::detectQtVersions()
{
    m_detectedQtVersions = scanForQtInstallations();
    emit detectedQtVersionsChanged();
    m_selectedQtVersions = m_detectedQtVersions;
    emit selectedQtVersionsChanged();
}

QStringList Cloner::scanForQtInstallations() const
{
    QStringList found = discoverQtInstallations().keys();

    // Sort by version number
    std::sort(found.begin(), found.end(), [](const QString &a, const QString &b) {
        auto partsA = a.split('.');
        auto partsB = b.split('.');
        for (int i = 0; i < qMin(partsA.size(), partsB.size()); ++i) {
            int na = partsA[i].toInt();
            int nb = partsB[i].toInt();
            if (na != nb)
                return na < nb;
        }
        return partsA.size() < partsB.size();
    });

    return found;
}

QByteArray Cloner::generateCMakePresets(const QStringList &versions) const
{
    static const QString vcpkgToolchain = QStringLiteral("$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake");
    static const QString dsqtInstallSuffix = QStringLiteral("$env{USERPROFILE}/Documents/DsQt");

    QJsonArray configurePresets;
    QJsonArray buildPresets;
    const auto qtInstallations = discoverQtInstallations();
    const auto visualStudio = discoverVisualStudioInstallation();
    const QString ninjaExecutable = discoverNinjaExecutable(visualStudio, qtInstallations);

    // vcpkg-base hidden preset
    QJsonObject vcpkgBase;
    vcpkgBase[QStringLiteral("name")] = QStringLiteral("vcpkg-base");
    vcpkgBase[QStringLiteral("hidden")] = true;
    vcpkgBase[QStringLiteral("toolchainFile")] = vcpkgToolchain;
    configurePresets.append(vcpkgBase);

    if (!ninjaExecutable.isEmpty()) {
        QJsonObject ninjaBase;
        ninjaBase[QStringLiteral("name")] = QStringLiteral("ninja-base");
        ninjaBase[QStringLiteral("hidden")] = true;
        ninjaBase[QStringLiteral("generator")] = QStringLiteral("Ninja Multi-Config");
        QJsonObject ninjaCache;
        ninjaCache[QStringLiteral("CMAKE_C_COMPILER")] = QStringLiteral("cl.exe");
        ninjaCache[QStringLiteral("CMAKE_CXX_COMPILER")] = QStringLiteral("cl.exe");
        ninjaCache[QStringLiteral("CMAKE_CXX_FLAGS_INIT")] = QStringLiteral("/EHsc");
        ninjaCache[QStringLiteral("CMAKE_CONFIGURATION_TYPES")]
            = QStringLiteral("Debug;Release;RelWithDebInfo");
        ninjaCache[QStringLiteral("CMAKE_MAKE_PROGRAM")] = ninjaExecutable;
        ninjaBase[QStringLiteral("cacheVariables")] = ninjaCache;
        configurePresets.append(ninjaBase);
    }

    for (const auto &ver : versions) {
        const QString cmakePath = qtInstallations.value(ver);
        if (cmakePath.isEmpty()) {
            qWarning() << "Skipping Qt" << ver << "because its installation path is unavailable";
            continue;
        }

        QString qtPresetName = QStringLiteral("qt-") + ver;
        QString prefixPath = cmakePath + QStringLiteral(";") + dsqtInstallSuffix;

        // Hidden Qt version preset
        QJsonObject qtPreset;
        qtPreset[QStringLiteral("name")] = qtPresetName;
        qtPreset[QStringLiteral("hidden")] = true;
        qtPreset[QStringLiteral("inherits")] = QStringLiteral("vcpkg-base");
        QJsonObject qtCache;
        qtCache[QStringLiteral("CMAKE_PREFIX_PATH")] = prefixPath;
        qtPreset[QStringLiteral("cacheVariables")] = qtCache;
        configurePresets.append(qtPreset);

        if (visualStudio.isValid()) {
            const QString vsName = visualStudio.presetPrefix + QStringLiteral("-") + ver;
            QJsonObject vsPreset;
            vsPreset[QStringLiteral("name")] = vsName;
            vsPreset[QStringLiteral("displayName")] = visualStudio.displayName
                                                              + QStringLiteral(" x64 - Qt ") + ver;
            vsPreset[QStringLiteral("inherits")] = qtPresetName;
            vsPreset[QStringLiteral("generator")] = visualStudio.generator;
            vsPreset[QStringLiteral("architecture")] = QStringLiteral("x64");
            vsPreset[QStringLiteral("binaryDir")] = QStringLiteral("${sourceDir}/build/") + vsName;
            QJsonObject vsCache;
            vsCache[QStringLiteral("CMAKE_GENERATOR_INSTANCE")] = visualStudio.installationPath;
            vsPreset[QStringLiteral("cacheVariables")] = vsCache;
            configurePresets.append(vsPreset);

            for (const auto &config : {std::make_pair("Debug", "debug"),
                                        std::make_pair("Release", "release"),
                                        std::make_pair("RelWithDebInfo", "relwithdebinfo")}) {
                QJsonObject bp;
                bp[QStringLiteral("name")] = vsName + QStringLiteral("-")
                                                     + QLatin1String(config.second);
                bp[QStringLiteral("displayName")] = QLatin1String(config.first);
                bp[QStringLiteral("configurePreset")] = vsName;
                bp[QStringLiteral("configuration")] = QLatin1String(config.first);
                buildPresets.append(bp);
            }
        }

        if (!ninjaExecutable.isEmpty()) {
            const QString ninjaName = QStringLiteral("ninja-") + ver;
            QJsonObject ninjaPreset;
            ninjaPreset[QStringLiteral("name")] = ninjaName;
            ninjaPreset[QStringLiteral("displayName")] = QStringLiteral("Ninja Multi-Config - Qt ")
                                                          + ver;
            ninjaPreset[QStringLiteral("inherits")] = QJsonArray{
                qtPresetName,
                QStringLiteral("ninja-base")
            };
            ninjaPreset[QStringLiteral("binaryDir")] = QStringLiteral("${sourceDir}/build/")
                                                        + ninjaName;
            configurePresets.append(ninjaPreset);

            for (const auto &config : {std::make_pair("Debug", "debug"),
                                        std::make_pair("Release", "release"),
                                        std::make_pair("RelWithDebInfo", "relwithdebinfo")}) {
                QJsonObject ninjaBp;
                ninjaBp[QStringLiteral("name")] = ninjaName + QStringLiteral("-")
                                                            + QLatin1String(config.second);
                ninjaBp[QStringLiteral("displayName")] = QLatin1String(config.first);
                ninjaBp[QStringLiteral("configurePreset")] = ninjaName;
                ninjaBp[QStringLiteral("configuration")] = QLatin1String(config.first);
                buildPresets.append(ninjaBp);
            }
        }
    }

    QJsonObject root;
    root[QStringLiteral("version")] = 6;
    root[QStringLiteral("configurePresets")] = configurePresets;
    root[QStringLiteral("buildPresets")] = buildPresets;

    QJsonDocument doc(root);
    return doc.toJson(QJsonDocument::Indented);
}

bool Cloner::writeCMakePresets(const QString &destDir, const QStringList &versions)
{
    QByteArray content = generateCMakePresets(versions);
    QString filePath = destDir + QStringLiteral("/CMakePresets.json");

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setLastError(QStringLiteral("Failed to write CMakePresets.json to ") + filePath);
        emit lastErrorSet();
        return false;
    }
    file.write(content);
    file.close();

    qInfo() << "Generated CMakePresets.json with" << versions.size() << "Qt version(s)";
    return true;
}

#include "cloner.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QThread>
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>

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
            success = success && replaceInFile(destDirectory.absoluteFilePath("CMakeLists.txt"),"PROJECT_DESC_",m_description);
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
    QStringList found;
    static const QRegularExpression versionRe(QStringLiteral(R"(^\d+\.\d+\.\d+$)"));

    QStringList searchRoots = {
        QStringLiteral("C:/Qt"),
        QStringLiteral("/mnt/c/Qt")
    };

    for (const auto &searchRoot : searchRoots) {
        QDir rootDir(searchRoot);
        if (!rootDir.exists())
            continue;

        auto entries = rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const auto &entry : entries) {
            if (!versionRe.match(entry).hasMatch())
                continue;

            QDir versionDir(rootDir.filePath(entry));
            if (!versionDir.exists())
                continue;

            QDir msvcDir(versionDir.filePath(QStringLiteral("msvc2022_64")));
            if (msvcDir.exists() && !found.contains(entry)) {
                found.append(entry);
            }
        }
    }

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

    // vcpkg-base hidden preset
    QJsonObject vcpkgBase;
    vcpkgBase[QStringLiteral("name")] = QStringLiteral("vcpkg-base");
    vcpkgBase[QStringLiteral("hidden")] = true;
    vcpkgBase[QStringLiteral("toolchainFile")] = vcpkgToolchain;
    configurePresets.append(vcpkgBase);

    for (const auto &ver : versions) {
        QString qtPresetName = QStringLiteral("qt-") + ver;
        QString cmakePath = QStringLiteral("C:/Qt/") + ver + QStringLiteral("/msvc2022_64");
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

        // VS2022 preset
        QString vsName = QStringLiteral("vs2022-") + ver;
        QJsonObject vsPreset;
        vsPreset[QStringLiteral("name")] = vsName;
        vsPreset[QStringLiteral("displayName")] = QStringLiteral("VS2022 x64 - Qt ") + ver;
        vsPreset[QStringLiteral("inherits")] = qtPresetName;
        vsPreset[QStringLiteral("generator")] = QStringLiteral("Visual Studio 17 2022");
        vsPreset[QStringLiteral("architecture")] = QStringLiteral("x64");
        vsPreset[QStringLiteral("binaryDir")] = QStringLiteral("${sourceDir}/build/") + vsName;
        configurePresets.append(vsPreset);

        // VS2022 build presets
        for (const auto &config : {std::make_pair("Debug", "debug"),
                                    std::make_pair("Release", "release"),
                                    std::make_pair("RelWithDebInfo", "relwithdebinfo")}) {
            QJsonObject bp;
            bp[QStringLiteral("name")] = vsName + QStringLiteral("-") + QLatin1String(config.second);
            bp[QStringLiteral("displayName")] = QLatin1String(config.first);
            bp[QStringLiteral("configurePreset")] = vsName;
            bp[QStringLiteral("configuration")] = QLatin1String(config.first);
            buildPresets.append(bp);
        }

        // Ninja presets
        for (const auto &config : {std::make_pair("Debug", "debug"),
                                    std::make_pair("Release", "release"),
                                    std::make_pair("RelWithDebInfo", "relwithdebinfo")}) {
            QString ninjaName = QStringLiteral("ninja-") + ver + QStringLiteral("-") + QLatin1String(config.second);

            QJsonObject ninjaPreset;
            ninjaPreset[QStringLiteral("name")] = ninjaName;
            ninjaPreset[QStringLiteral("displayName")] = QStringLiteral("Ninja ") + QLatin1String(config.first) + QStringLiteral(" - Qt ") + ver;
            ninjaPreset[QStringLiteral("inherits")] = qtPresetName;
            ninjaPreset[QStringLiteral("generator")] = QStringLiteral("Ninja");
            ninjaPreset[QStringLiteral("binaryDir")] = QStringLiteral("${sourceDir}/build/") + ninjaName;
            QJsonObject ninjaCache;
            ninjaCache[QStringLiteral("CMAKE_BUILD_TYPE")] = QLatin1String(config.first);
            ninjaCache[QStringLiteral("CMAKE_C_COMPILER")] = QStringLiteral("cl.exe");
            ninjaCache[QStringLiteral("CMAKE_CXX_COMPILER")] = QStringLiteral("cl.exe");
            ninjaPreset[QStringLiteral("cacheVariables")] = ninjaCache;
            configurePresets.append(ninjaPreset);

            QJsonObject ninjaBp;
            ninjaBp[QStringLiteral("name")] = ninjaName;
            ninjaBp[QStringLiteral("configurePreset")] = ninjaName;
            buildPresets.append(ninjaBp);
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

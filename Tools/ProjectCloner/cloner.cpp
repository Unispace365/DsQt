#include "cloner.h"
#include <QFileInfo>
#include <QThread>
#include <QString>

Cloner::Cloner() {
    checker = new GitIgnoreChecker("skip.txt");
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



            //run through the CMakeLists.txt and Readme.md making substitutions as needed.
            //Main.cpp
            bool success = true;
            success && replaceInFile(destDirectory.absoluteFilePath("main.cpp"),"ClonerSource",m_applicationName);
            success && replaceInFile(destDirectory.absoluteFilePath("CMakeLists.txt"),"ClonerSource",m_applicationName);
            success && replaceInFile(destDirectory.absoluteFilePath("CMakeLists.txt"),"PROJECT_NAME_",m_projectName);
            success && replaceInFile(destDirectory.absoluteFilePath("CMakeLists.txt"),"PROJECT_DESC_",m_description);
            success && replaceInFile(destDirectory.absoluteFilePath("README.md"),"PROJECT_NAME_",m_projectName);
            success && replaceInFile(destDirectory.absoluteFilePath("README.md"),"APP_NAME_",m_applicationName);
            if(!success) {
                setStatus(Status::POSTCLONE);
                return;
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

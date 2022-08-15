#ifndef DSSETTINGS_H
#define DSSETTINGS_H


#include <QObject>
#include <qqml.h>
#include <toml++/toml.h>

/**
 * @brief The DSSettings class
 * Reads a setting file
 */
class DSSettings;
typedef std::shared_ptr<DSSettings> DSSettingsRef;

class DSSettings:public QObject
{
    Q_OBJECT

    QML_ELEMENT
public:
    DSSettings(QObject* parent=nullptr);
    static DSSettingsRef createFromSettingFile(QString* file,QObject* parent=nullptr);
private:
    toml::parse_result mResult;
};

#endif // DSSETTINGS_H

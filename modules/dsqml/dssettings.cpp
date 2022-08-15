#include "dssettings.h"
#include <toml++/toml.h>

DSSettings::DSSettings(QObject* parent):QObject(parent)
{
}

DSSettingsRef DSSettings::createFromSettingFile(QString *file, QObject *parent)
{
    DSSettingsRef retVal = DSSettingsRef(new DSSettings(parent));
    retVal->mResult = toml::parse_file(file->toStdString());
    return retVal;

}

#ifndef DSENGINESETTINGS_H
#define DSENGINESETTINGS_H

#include "dssettings.h"
namespace dsqt {
class DSEngineSettings : public DSSettings
{
public:
    explicit DSEngineSettings(QObject *parent = nullptr);

    std::string sProjectPath="";
};

}//namespace dsqt;

#endif // DSENGINESETTINGS_H

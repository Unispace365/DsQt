#ifndef DSSETTINGS_H
#define DSSETTINGS_H

#include <toml++/toml.h>
#include <QObject>
#include <qqml.h>

/**
 * @brief The DSSettings class
 * Reads a setting file
 */

class DSSettings:public QObject
{
    Q_OBJECT

    QML_ELEMENT
public:
    DSSettings(QObject* parent=nullptr);

};

#endif // DSSETTINGS_H

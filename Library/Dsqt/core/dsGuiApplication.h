
#ifndef DSGUIAPPLICATION_H
#define DSGUIAPPLICATION_H

#include <QGuiApplication>



class DsGuiApplication : public QGuiApplication
{
  public:
    DsGuiApplication(int &argc, char **argv);
};

#endif // DSGUIAPPLICATION_H

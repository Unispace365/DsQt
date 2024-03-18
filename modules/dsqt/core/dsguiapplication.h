
#ifndef DSGUIAPPLICATION_H
#define DSGUIAPPLICATION_H

#include <QGuiApplication>



class DSGuiApplication : public QGuiApplication
{
  public:
    DSGuiApplication(int &argc, char **argv);
};

#endif // DSGUIAPPLICATION_H

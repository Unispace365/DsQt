#ifndef DSQMLAPPLICATIONENGINE_H
#define DSQMLAPPLICATIONENGINE_H

#include <QQmlApplicationEngine>
#include <QObject>

class DSQmlApplicationEngine : public QQmlApplicationEngine
{
    Q_OBJECT
public:
    explicit DSQmlApplicationEngine(QObject *parent = nullptr);
};

#endif // DSQMLAPPLICATIONENGINE_H

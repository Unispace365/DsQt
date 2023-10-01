#ifndef DSNODEWATCHER_H
#define DSNODEWATCHER_H

#include "core/dsqmlapplicationengine.h"

#include <QMutex>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>
#include <QQmlApplicationEngine>

Q_DECLARE_LOGGING_CATEGORY(nodeWatcher)

namespace dsqt::network {

class Message {
public:
    Message();

    /// A stack of all the messages received
    std::vector<QString> mData;

    bool empty() const;
    void clear();
    void swap(Message&);
};

class Loop: public QObject {
    Q_OBJECT
public:
    Loop(DSQmlApplicationEngine* engine,const QString host, const int port);
    ~Loop();
public:
    QMutex      mMutex;
    bool		mAbort;
    Message		mMsg;

signals:
    void messageAvailable();




private:
    const QString   mHost;
    const int       mPort;
    const long      mRefreshRateMs; // in milliseconds
    DSQmlApplicationEngine* mEngine;

    // QRunnable interface
public:
    virtual void run();
};

class DsNodeWatcher : public QObject
{
    Q_OBJECT
    QML_ELEMENT
public:
    DsNodeWatcher(DSQmlApplicationEngine *parent,QString host="localhost",int port=7788,bool autoStart=true);

    void handleMessage();
    void start();
    void stop();

public:
    /// A generic class that stores info received from the node.

signals:
    void messageArrived(Message msg);
    void stopped();
    void started();

private:
    Message mMsg;
    Loop    mLoop;
    QFuture<void> mWatcher;
    QMutex mMutex;
    DSQmlApplicationEngine* mEngine;
};
}

#endif // DSNODEWATCHER_H

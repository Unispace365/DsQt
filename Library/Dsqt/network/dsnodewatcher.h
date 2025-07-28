#ifndef DSNODEWATCHER_H
#define DSNODEWATCHER_H

#include "core/dsQmlApplicationEngine.h"

#include <QFuture>
#include <QMutex>
#include <QQmlApplicationEngine>
#include <QUdpSocket>
#include <QtConcurrent/QtConcurrent>

Q_DECLARE_LOGGING_CATEGORY(lgNodeWatcher)
Q_DECLARE_LOGGING_CATEGORY(lgNodeWatcherVerbose)

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
    Loop(DsQmlApplicationEngine* engine,const QString host, const int port);
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
    DsQmlApplicationEngine* mEngine;

    // QRunnable interface
public:
    virtual void run();
};

class DsNodeWatcher : public QObject
{
    Q_OBJECT

public:
  DsNodeWatcher(DsQmlApplicationEngine* parent = nullptr, QString host = "localhost", int port = 7788, bool autoStart = true);

  void handleMessage();
  void start();
  void stop();

public:
    /// A generic class that stores info received from the node.

signals:
    void messageArrived(dsqt::network::Message msg);
    void stopped();
    void started();

  public slots:
	void processPendingDatagrams();

  private:
	Message mMsg;
	Loop	mLoop;
	int		mPort;

	QFuture<void>			mWatcher;
	QMutex					mMutex;
	DsQmlApplicationEngine* mEngine;
	QUdpSocket*				mSocket = nullptr;
};
}

#endif // DSNODEWATCHER_H

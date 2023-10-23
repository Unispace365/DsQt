#include "dsnodewatcher.h"

#include <QHostAddress>
#include <QTcpSocket>
#include <QUdpSocket>
namespace dsqt::network {
DsNodeWatcher::DsNodeWatcher(DSQmlApplicationEngine* parent,QString url,int port,bool autoStart):
    QObject(parent),
    mLoop(parent,url,port),
    mEngine(parent)
{

    if(autoStart){
        start();
    }

}

void DsNodeWatcher::handleMessage()
{
	qDebug()<<"in handleMessage "<<QThread::currentThreadId();
    mMsg.clear();
    {
        QMutexLocker l(&mLoop.mMutex);
        mMsg.swap(mLoop.mMsg);
    }
    emit messageArrived(mMsg);
}

void DsNodeWatcher::start()
{
    if(mWatcher.isRunning()) return;
    connect(
        &mLoop,&Loop::messageAvailable,
        this,&DsNodeWatcher::handleMessage,
        static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::UniqueConnection) );
    mWatcher = QtConcurrent::run(&Loop::run,&mLoop);

}

void DsNodeWatcher::stop()
{
    QMutexLocker l(&mLoop.mMutex);
    mLoop.mAbort = true;
}


Message::Message()
{

}

bool Message::empty() const
{

    return mData.empty();
}

void Message::clear()
{
    mData.clear();
}

void Message::swap(Message &o)
{
    mData.swap(o.mData);
}

static long get_refresh_rate(DSQmlApplicationEngine* engine){
    auto settings = engine->getSettings();
    float rate = settings->getOr("node.refresh_rate",.1f);
    long ans = static_cast<long>(rate * 1000.0f);
    if(ans<10) {
        return 10;
    } else if (ans > 1000 * 10) {
        return 1000 * 10;
    }
    return ans;
}

Loop::Loop(DSQmlApplicationEngine* engine,const QString host, const int port):QObject(engine),
    mAbort(false),
    mHost(host),
    mPort(port),
    mRefreshRateMs(get_refresh_rate(engine)),
    mEngine(engine)
{

}

Loop::~Loop()
{

}

void Loop::run()
{
    static const int    BUF_SIZE = 512;
	char                buf[BUF_SIZE];

	QUdpSocket* theSocket = new QUdpSocket();
	QObject* connectionLife = new QObject();;
	try {
		connect(theSocket, &QUdpSocket::connected, this, [&](){
				qDebug()<<"connected";
			});
		connect(theSocket, &QUdpSocket::errorOccurred, this, [&](){
				qDebug()<<"error";
			});
		connect(theSocket,&QUdpSocket::readyRead,this,[&](){
			qDebug()<<"Got Message in Lambda"<<QThread::currentThreadId();
			int length = theSocket->readDatagram(buf,BUF_SIZE);
			if(length > 0){
				QString msg = QString::fromUtf8(buf,length);
				QMutexLocker l(&mMutex);
				mMsg.mData.emplace_back(msg);
			}

		});

		theSocket->bind(QHostAddress::LocalHost,7788,QUdpSocket::ReuseAddressHint);
		//theSocket->open(QIODevice::ReadOnly);

		//theSocket->setTextModeEnabled(true);
		//theSocket->connectToHost(QHostAddress::LocalHost,mPort);
		//qDebug()<<"socket: "<<theSocket->set();




        while(true){
			while(theSocket->hasPendingDatagrams()){
				int length = theSocket->readDatagram(buf,BUF_SIZE);
				if(length > 0){
					QString msg = QString::fromUtf8(buf,length);
					QMutexLocker l(&mMutex);
					mMsg.mData.emplace_back(msg);
				}

			}
			//qDebug()<<"looping";
            if(mMsg.mData.size()>0){
				qDebug()<<"emitting messageAvailable "<<QThread::currentThreadId();
                emit messageAvailable();
            }

            QThread::msleep(mRefreshRateMs);
            {
                QMutexLocker l(&mMutex);
                if(mAbort) break;
            }

        }
	} catch(QException ex){

    }
}

}

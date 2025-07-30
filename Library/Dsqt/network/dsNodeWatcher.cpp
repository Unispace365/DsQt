#include "dsNodeWatcher.h"

#include <QHostAddress>
#include <QTcpSocket>
#include <QUdpSocket>
#include "core/dsEnvironment.h"

Q_LOGGING_CATEGORY(lgNodeWatcher, "network.nodewatcher")
Q_LOGGING_CATEGORY(lgNodeWatcherVerbose, "network.nodewatcher.verbose")

namespace dsqt::network {
DsNodeWatcher::DsNodeWatcher(DsQmlApplicationEngine* parent, QString url, int port, bool autoStart)
  : QObject(parent), mLoop(parent, url, port), mPort(port), mEngine(parent) {

	if (autoStart) {
		start();
	}
}

void DsNodeWatcher::handleMessage()
{
    mMsg.clear();
    {
        QMutexLocker l(&mLoop.mMutex);
        mMsg.swap(mLoop.mMsg);
    }
    emit messageArrived(mMsg);
}

void DsNodeWatcher::start()
{

	if (mSocket) {
		mSocket->close();
		delete mSocket;
	}

    mSocket		= new QUdpSocket(this);
    bool result = mSocket->bind(QHostAddress::LocalHost, mPort);
    processPendingDatagrams();
    connect(mSocket, &QUdpSocket::readyRead, this, &DsNodeWatcher::processPendingDatagrams, Qt::QueuedConnection);

    //if (mWatcher.isRunning()) return;
    qInfo() << "Starting NodeWatcher";
    connect(&mLoop, &Loop::messageAvailable, this, &DsNodeWatcher::handleMessage,
    static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::UniqueConnection));
    //mWatcher = QtConcurrent::run(&Loop::run, &mLoop);
}

void DsNodeWatcher::processPendingDatagrams() {

	QHostAddress sender;
	uint16_t	 port;
	mMsg.clear();
	QByteArray datagram;
    while (mSocket->hasPendingDatagrams()) {

		datagram.resize(mSocket->pendingDatagramSize());
		mSocket->readDatagram(datagram.data(), datagram.size(), &sender, &port);
        if(sender == QHostAddress::LocalHost){
            qCDebug(lgNodeWatcherVerbose) << "Message From :: " << sender.toString();
            qCDebug(lgNodeWatcherVerbose) << "Port From :: " << port;
            qCDebug(lgNodeWatcherVerbose) << "Message :: " << datagram;
            mMsg.mData.emplace_back(QString::fromUtf8(datagram));
        }
	}
	datagram.resize(mSocket->pendingDatagramSize());
	mSocket->readDatagram(datagram.data(), datagram.size(), &sender, &port);
	if (mMsg.mData.size() > 0) {
		emit messageArrived(mMsg);
	}
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

static long get_refresh_rate(DsQmlApplicationEngine* engine){
	auto  settings = DsEnvironment::engineSettings();
	float rate	   = settings->getOr("node.refresh_rate", .1f);
	long  ans	   = static_cast<long>(rate * 1000.0f);
	if (ans < 10) {
		return 10;
	} else if (ans > 1000 * 10) {
		return 1000 * 10;
	}
	return ans;
}

Loop::Loop(DsQmlApplicationEngine* engine,const QString host, const int port):QObject(engine),
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
    
    QUdpSocket theSocket;
	try {

		QObject connectionLife;


        auto good = theSocket.bind(QHostAddress::Any, mPort);
        if (!good) {
			qCWarning(lgNodeWatcher) << "Failed to bind to port:" << mPort << "(" << theSocket.errorString() << ")";
		} else {
            qCInfo(lgNodeWatcher) << "Bound to port:" << mPort;
		}

        mEngine->connect(&theSocket,&QUdpSocket::readyRead,&connectionLife,[&](){

            int length = theSocket.readDatagram(buf,BUF_SIZE);
            if(length > 0){
                QString msg = QString::fromUtf8(buf,length);
                QMutexLocker l(&mMutex);
                mMsg.mData.emplace_back(msg);
            }

        });

        QString prevPending="false";
		while (true) {
            auto pending = theSocket.hasPendingDatagrams()?"true":"false";
            if(pending != prevPending){
                qDebug()<<"pending = "<<pending;
                prevPending = pending;
            }
            if (mMsg.mData.size() > 0) {
                emit messageAvailable();
			}

			QThread::msleep(mRefreshRateMs);
			{
				QMutexLocker l(&mMutex);
				if (mAbort) break;
			}
		}
    } catch (std::exception e) {
        qCWarning(lgNodeWatcher)<<"exception in node watcher";
    }
}

}

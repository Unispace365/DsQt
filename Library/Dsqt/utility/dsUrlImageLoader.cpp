#include "utility/dsUrlImageLoader.h"

namespace dsqt {

DsUrlImageLoader::DsUrlImageLoader(QObject* parent) : QObject(parent), m_manager(new QNetworkAccessManager(this)) {
    connect(m_manager, &QNetworkAccessManager::finished, this, &DsUrlImageLoader::onFinished);
}

void DsUrlImageLoader::loadImage(const QUrl& url) {
	QNetworkRequest request(url);
	QNetworkReply*	reply = m_manager->get(request);

    connect(reply, &QNetworkReply::downloadProgress, this, &DsUrlImageLoader::downloadProgress);
    connect(reply, &QNetworkReply::finished, this, &DsUrlImageLoader::onFinished);

	emit downloadStarted();
}

QImage dsqt::DsUrlImageLoader::loadImageSync(const QUrl& url) {
	QNetworkRequest request(url);
	QNetworkReply*	reply = m_manager->get(request);

	QEventLoop loop;
	connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();

	if (reply->error()) {
		qDebug() << "Error:" << reply->errorString();
		return QImage();
	}

	QByteArray data = reply->readAll();
	QImage	   image;
	image.loadFromData(data);

	reply->deleteLater();

	return image;
}

void DsUrlImageLoader::onFinished() {
	QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
	if (reply->error()) {
		emit errorOccurred(reply->errorString());
	} else {
		QByteArray data = reply->readAll();
		QImage	   image;
		image.loadFromData(data);
		emit downloadFinished(image);
	}

	reply->deleteLater();
}

}  // namespace dsqt

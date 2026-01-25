#ifndef DSURLIMAGELOADER_H
#define DSURLIMAGELOADER_H

#include <QEventLoop>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>

namespace dsqt {
class DsUrlImageLoader : public QObject {
	Q_OBJECT

  public:
    explicit DsUrlImageLoader(QObject *parent = nullptr);

	void loadImage(const QUrl &url);

	QImage loadImageSync(const QUrl &url);

  signals:
	void downloadStarted();
	void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
	void downloadFinished(QImage image);
	void errorOccurred(QString errorString);

  private slots:
	void onFinished();

  private:
	QNetworkAccessManager *m_manager;
};

}  // namespace dsqt


#endif	// IMAGELOADER_H

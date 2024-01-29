#ifndef DSRESOURCE_H
#define DSRESOURCE_H

#include <QObject>
#include <QtQml/qqml.h>
#include <string_view>

namespace dsqt {
using namespace std::literals::string_view_literals;
class DSResource;
//using DSResourceRef = std::shared_ptr<DSResource>;
class DSResource : public QObject
{

	Q_OBJECT
  public:
	enum class ResourceType {
		ERROR,
		FONT,
		IMAGE,
		IMAGE_SEQUENCE,
		PDF,
		VIDEO,
		WEB,
		VIDEO_STREAM,
		ZIP,
		VIDEO_PANORAMIC,
		YOUTUBE
	};
	Q_ENUM(ResourceType)
	enum class Origin {
		UNKNOWN,
		NETWORK,
		LOCAL
	};
	Q_ENUM(Origin)
	static const QMap<QString,ResourceType> sDbToTypeMap;


  private:
	Q_PROPERTY(QUrl url READ url WRITE setUrl NOTIFY urlChanged)
	Q_PROPERTY(float width READ width WRITE setWidth NOTIFY widthChanged)
	Q_PROPERTY(float height READ height WRITE setHeight NOTIFY heightChanged)
	Q_PROPERTY(double duration READ duration WRITE setDuration NOTIFY durationChanged)
	Q_PROPERTY(ResourceType resourceType READ resourceType WRITE setResourceType NOTIFY resourceTypeChanged)
	Q_PROPERTY(long id READ id WRITE setId NOTIFY idChanged)
	Q_PROPERTY(Origin origin READ origin WRITE setOrigin NOTIFY originChanged)
	QML_ELEMENT
  public:
	explicit DSResource(QObject *parent = nullptr);
	explicit DSResource(int id,ResourceType type,float duration, float width, float height, QString url, int thumb_id,QObject *parent = nullptr);

	//static DSResourceRef create(int id,ResourceType type, float duration, float width, float heightalso0, QString url,int thumb_id,QObject* parent);
	bool operator==(const DSResource&) const;
	bool operator!=(const DSResource&) const;
	QUrl url() const;
	void setUrl(const QUrl &newUrl);

	float width() const;
	void setWidth(float newWidth);

	float height() const;
	void setHeight(float newHeight);

	double duration() const;
	void setDuration(double newDuration);

	ResourceType resourceType() const;
	void setResourceType(ResourceType newResourceType);

	long id() const;
	void setId(long newId);

	Origin origin() const;
	void setOrigin(Origin newOrigin);

	void setTypeFromString(const QString& typeChar);

	static const ResourceType makeTypeFromString(const QString& typeChar);

  private:
	long mId;
	ResourceType mResourceType;
	double mDuration;
	float mWidth, mHeight;
	QString mUrlString;
	QUrl mUrl;
	long mThumbnailId;
	std::string mThumbnailFilePath;
	Origin mOrigin = Origin::UNKNOWN;

	//child resources. Maybe just use QObject for this?
	long mParentId;
	long mParentIndex;
	std::vector<DSResource> mChildResources;



  signals:

	void urlChanged();
	void widthChanged();
	void heightChanged();
	void originChanged();
	void durationChanged();
	void resourceTypeChanged();
	void idChanged();
};

class DSResourceRef {

  private:
	std::shared_ptr<DSResource> mData;
};
} //namespace dsqt
#endif // DSRESOURCE_H

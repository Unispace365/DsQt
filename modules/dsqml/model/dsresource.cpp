#include "dsresource.h"
namespace dsqt {

const QMap<QString,DSResource::ResourceType> DSResource::sDbToTypeMap{
	{"f", DSResource::ResourceType::FONT},
	{"i", DSResource::ResourceType::IMAGE},
	{"is", DSResource::ResourceType::IMAGE_SEQUENCE},
	{"p", DSResource::ResourceType::PDF},
	{"v", DSResource::ResourceType::VIDEO},
	{"s", DSResource::ResourceType::VIDEO_STREAM},
	{"w", DSResource::ResourceType::WEB},
	{"y", DSResource::ResourceType::YOUTUBE},
	{"z", DSResource::ResourceType::ZIP},
	{"0", DSResource::ResourceType::ERROR}
};

DSResource::DSResource(QObject *parent)
  : QObject{parent}
{

}

DSResource::DSResource(int id, ResourceType type, float duration, float width, float height, QString url, int thumb_id, QObject *parent) :
  QObject(parent),
  mId(id),
  mResourceType(type),
  mDuration(duration),
  mWidth(width),
  mHeight(height),
  mUrl(QUrl(url)),
  mThumbnailId(thumb_id)
{

}

DSResourceRef DSResource::create(int id, ResourceType type, float duration, float width, float height,QString url, int thumb_id, QObject *parent)
{
	return DSResourceRef(new DSResource(id,type,duration,width,height,url,thumb_id,parent));
}

bool DSResource::operator==(const DSResource & other) const
{
	if( mUrl == other.mUrl &&
		mResourceType == other.mResourceType &&
		mDuration == other.mDuration &&
		mWidth == other.mWidth &&
		mHeight == other.mHeight){
		return true;
	}
	return false;
}

bool DSResource::operator!=(const DSResource & other) const
{
	if(*this == other){
		return false;
	}
	return true;
}

QUrl DSResource::url() const
{
	return mUrl;
}

void DSResource::setUrl(const QUrl &newUrl)
{
	if (mUrl == newUrl)
		return;
	mUrl = newUrl;
	emit urlChanged();
}

float DSResource::width() const
{
	return mWidth;
}

void DSResource::setWidth(float newWidth)
{
	if (qFuzzyCompare(mWidth, newWidth))
		return;
	mWidth = newWidth;
	emit widthChanged();
}

float DSResource::height() const
{
	return mHeight;
}

void DSResource::setHeight(float newHeight)
{
	if (qFuzzyCompare(mHeight, newHeight))
		return;
	mHeight = newHeight;
	emit heightChanged();
}


double DSResource::duration() const
{
	return mDuration;
}

void DSResource::setDuration(double newDuration)
{
	if (qFuzzyCompare(mDuration, newDuration))
		return;
	mDuration = newDuration;
	emit durationChanged();
}

DSResource::ResourceType DSResource::resourceType() const
{
	return mResourceType;
}

void DSResource::setResourceType(ResourceType newResourceType)
{
	if (mResourceType == newResourceType)
		return;
	mResourceType = newResourceType;
	emit resourceTypeChanged();
}

long DSResource::id() const
{
	return mId;
}

void DSResource::setId(long newId)
{
	if (mId == newId)
		return;
	mId = newId;
	emit idChanged();
}

DSResource::Origin DSResource::origin() const
{
	return mOrigin;
}

void DSResource::setOrigin(Origin newOrigin)
{
	if (mOrigin == newOrigin)
		return;
	mOrigin = newOrigin;
	emit originChanged();
}

void DSResource::setTypeFromString(const QString& typeChar) {
	setResourceType(makeTypeFromString(typeChar));
}


const DSResource::ResourceType DSResource::makeTypeFromString(const QString& typeChar) {
	if (sDbToTypeMap.find(typeChar)!=sDbToTypeMap.end())
		return sDbToTypeMap[typeChar];
	else
		return DSResource::ResourceType::ERROR;
}

} //namespace dsqt

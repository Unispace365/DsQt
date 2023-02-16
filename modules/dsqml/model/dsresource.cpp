#include "dsresource.h"
namespace dsqt {
using RST = DSResource::ResourceType;
const QMap<QStringView,DSResource::ResourceType> DSResource::sDbToTypeMap =
{{
	{"f"sv, RST::FONT},
	{"i"sv, RST::IMAGE},
	{"is"sv, RST::IMAGE_SEQUENCE},
	{"p"sv, RST::PDF},
	{"v"sv, RST::VIDEO},
	{"s"sv, RST::VIDEO_STREAM},
	{"w"sv, RST::WEB},
	{"y"sv, RST::YOUTUBE},
	{"z"sv, RST::ZIP},
	{"0"sv, RST::ERROR}
}};

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
		return RST::ERROR;
}

} //namespace dsqt



#include "model/resource.h"
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <iostream>
#include <sstream>
#include "core/dsenvironment.h"
#include "utility/file_meta_data.h"

QString QSSS(const std::string& in) {
	return QString::fromStdString(in);
}
std::string SSQS(const QString& in) {
	return in.toStdString();
}

namespace fs = std::filesystem;
Q_LOGGING_CATEGORY(lgResource, "model.resource")
namespace {
const QString FONT_TYPE_SZ("f");
const QString IMAGE_TYPE_SZ("i");
const QString IMAGE_SEQUENCE_TYPE_SZ("is");
const QString PDF_TYPE_SZ("p");
const QString VIDEO_TYPE_SZ("v");
const QString VIDEO_STREAM_TYPE_SZ("s");
const QString WEB_TYPE_SZ("w");
const QString YOUTUBE_TYPE_SZ("y");
const QString ZIP_TYPE_SZ("z");
const QString ERROR_TYPE_SZ("0");

// This gets reduced to being a video type; here to support B&R CMSs, which
// can't have audio files that are typed as video.
const QString AUDIO_TYPE_SZ("a");

const QString EMPTY_SZ("");

const std::wstring FONT_NAME_SZ(L"font");
const std::wstring IMAGE_NAME_SZ(L"image");
const std::wstring IMAGE_SEQUENCE_NAME_SZ(L"image sequence");
const std::wstring PDF_NAME_SZ(L"pdf");
const std::wstring VIDEO_NAME_SZ(L"video");
const std::wstring VIDEO_STREAM_NAME_SZ(L"video stream");
const std::wstring WEB_NAME_SZ(L"web");
const std::wstring YOUTUBE_NAME_SZ(L"youtube");
const std::wstring ZIP_NAME_SZ(L"zip");
const std::wstring ERROR_NAME_SZ(L"error");
} // namespace

namespace dsqt {

/**
 * ds::Resource::Id
 */
Resource::Id::Id()
  : mType(CMS_TYPE)
  , mValue(0) {}

Resource::Id::Id(const qint64 value) : mType(CMS_TYPE), mValue(value) {}

Resource::Id::Id(const QString value) : mType(CMS_TYPE), mValue(qHash(value)) {}

Resource::Id::Id(const char type, const qint64 value) : mType(type), mValue(value) {}

Resource::Id::Id(const char type, const QString value) : mType(type), mValue(qHash(value)) {}

bool Resource::Id::operator==(const Id& o) const {
	return mType == o.mType && mValue == o.mValue;
}

bool Resource::Id::operator!=(const Id& o) const {
	return mType != o.mType || mValue != o.mValue;
}

bool Resource::Id::operator>(const Id& o) const {
	if (mType == o.mType) return mValue > o.mValue;
	return mType > o.mType;
}

bool Resource::Id::operator<(const Id& o) const {
	if (mType == o.mType) return mValue < o.mValue;
	return mType < o.mType;
}

bool Resource::Id::operator>(const qint64 value) const {
	return mValue > value;
}

bool Resource::Id::operator>=(const qint64 value) const {
	return mValue >= value;
}

bool Resource::Id::operator<(const qint64 value) const {
	return mValue < value;
}

bool Resource::Id::operator<=(const qint64 value) const {
	return mValue <= value;
}

bool Resource::Id::operator>(const QString value) const {
	return mValue > qHash(value);
}

bool Resource::Id::operator>=(const QString value) const {
	return mValue >= qHash(value);
}

bool Resource::Id::operator<(const QString value) const {
	return mValue < qHash(value);
}

bool Resource::Id::operator<=(const QString value) const {
	return mValue <= qHash(value);
}

bool Resource::Id::empty() const {
	// Database IDs must start with 1, so that's our notion of valid.
	return mValue < 1;
}

void Resource::Id::clear() {
	*this = Id();
}

void Resource::Id::swap(Id& id) {
	std::swap(mType, id.mType);
	std::swap(mValue, id.mValue);
}

static bool try_parse(const QString& s, const QString& typeStr, const char type, Resource::Id& out) {

	if (s.startsWith(typeStr) != true) return false;
	if (s.length() <= typeStr.size()) return false;

	out.mType  = type;
	out.mValue = atoi(s.toStdString().c_str() + typeStr.size());
	return true;
}

bool Resource::Id::tryParse(const QString& s) {
	static const char		 TYPE_A[] = {CMS_TYPE, APP_TYPE};
	static const QString	 TAG_A[]  = {"cms:", "app:"};
	for (int k = 0; k < 2; ++k) {
		Id v;
		if (try_parse(s, TAG_A[k], TYPE_A[k], v)) {
			*this = v;
			return true;
		}
	}
	qCWarning(lgResource) << "ERROR dsqt::Resource::Id::tryParse() illegal input (" << s << ")";
	return false;
}

bool Resource::Id::verifyPaths() const {
	if (getResourcePath().isEmpty()) {
		//		DS_LOG_ERROR_M("resource_id (" << *this << ") missing resource path", ds::GENERAL_LOG);
		return false;
	}
	if (getDatabasePath().isEmpty()) {
		//		DS_LOG_ERROR_M("resource_id (" << *this << ") missing database path", ds::GENERAL_LOG);
		return false;
	}
	return true;
}

/*
void Resource::Id::writeTo(DataBuffer& buf) const {
	buf.add(mType);
	buf.add(mValue);
}

bool Resource::Id::readFrom(DataBuffer& buf) {
	if (!buf.canRead<char>()) return false;
	mType = buf.read<char>();
	if (!buf.canRead<int>()) return false;
	mValue = buf.read<int>();
	return true;
}
*/

/**
 * ds::Resource::id database path
 */
namespace {
QString		  CMS_RESOURCE_PATH("");
QString		  CMS_PORTABLE_RESOURCE_PATH("");
QString		  CMS_DB_PATH("");
QString		  APP_RESOURCE_PATH("");
QString		  APP_DB_PATH("");
const QString EMPTY_PATH("");
// Function for generating custom paths
std::function<const QString&(const Resource::Id&)> CUSTOM_RESOURCE_PATH;
std::function<const QString&(const Resource::Id&)> CUSTOM_DB_PATH;
} // namespace

const QString& Resource::Id::getResourcePath() const {
	if (mType == CMS_TYPE) return CMS_RESOURCE_PATH;
	if (mType == APP_TYPE) return APP_RESOURCE_PATH;
	if (mType <= CUSTOM_TYPE && CUSTOM_RESOURCE_PATH) return CUSTOM_RESOURCE_PATH(*this);
	return EMPTY_PATH;
}

const QString& Resource::Id::getDatabasePath() const {
	if (mType == CMS_TYPE) return CMS_DB_PATH;
	if (mType == APP_TYPE) return APP_DB_PATH;
	if (mType <= CUSTOM_TYPE && CUSTOM_DB_PATH) return CUSTOM_DB_PATH(*this);
	return EMPTY_PATH;
}

const QString& Resource::Id::getPortableResourcePath() const {
	if (mType == CMS_TYPE) return CMS_PORTABLE_RESOURCE_PATH;
	if (mType <= CUSTOM_TYPE && CUSTOM_RESOURCE_PATH) return CUSTOM_RESOURCE_PATH(*this);
	return EMPTY_PATH;
}


void Resource::Id::setupPaths(const QString& resource, const QString& db, const QString& projectPath) {
	setupPaths(resource.toStdString(), db.toStdString(), projectPath.toStdString());
}

void Resource::Id::setupPaths(const std::string& resource, const std::string& db, const std::string& projectPath) {
	// todo: switch this function to use std::filesystem
	CMS_RESOURCE_PATH = QString::fromStdString(resource);
	{
		fs::path p(resource);
		p.append(db);

		CMS_DB_PATH = QString::fromStdString(p.lexically_normal().string());
	}

	// Portable path. We want it as small as possible to ease network traffic.
	std::string local = dsqt::DSEnvironment::expand("%LOCAL%");
	std::string cmsPortableResourcePath;
	if (resource.rfind(local, 0) == 0) {
		cmsPortableResourcePath = "%LOCAL%";
		cmsPortableResourcePath.append(resource.substr(local.size()));
	} else {
		// Not an error
		// DS_LOG_ERROR("CMS resource path (" << CMS_RESOURCE_PATH << ") does not start with %LOCAL% (" << local <<
		// ")");
		cmsPortableResourcePath = resource;
	}

	cmsPortableResourcePath	   = fs::path(cmsPortableResourcePath).lexically_normal().string();
	CMS_PORTABLE_RESOURCE_PATH = QString::fromStdString(cmsPortableResourcePath);

	// If the project path exists, then setup our app-local resources path.
	if (!projectPath.empty()) {
		fs::path p((DSEnvironment::getDownstreamDocumentsFolder()));
		p.append("resources");
		p.append(projectPath);
		p.append("app");
		APP_RESOURCE_PATH = QString::fromStdString(p.lexically_normal().string());

		p.append("db");
		p.append("db.sqlite");
		APP_DB_PATH = QString::fromStdString(p.string());

		// Make sure we have the trailing separator on the resource path.
		if (!APP_RESOURCE_PATH.isEmpty() && APP_RESOURCE_PATH.back() != fs::path::preferred_separator) {
			APP_RESOURCE_PATH.append(fs::path::preferred_separator);
		}
	}

	qCInfo(lgResource) << "CMS_RESOURCE_PATH: " << CMS_RESOURCE_PATH;
	qCInfo(lgResource) << "CMS_PORTABLE_RESOURCE_PATH: " << CMS_PORTABLE_RESOURCE_PATH;
	qCInfo(lgResource) << "APP_RESOURCE_PATH: " << APP_RESOURCE_PATH;
}

void Resource::Id::setupCustomPaths(const std::function<const QString&(const Resource::Id&)>& resourcePath,
									const std::function<const QString&(const Resource::Id&)>& dbPath) {
	CUSTOM_RESOURCE_PATH = resourcePath;
	CUSTOM_DB_PATH		 = dbPath;
}

/**
 * ds::Resource
 */
Resource Resource::fromImage(const QString& full_path) {
	Resource r;
	r.mType			 = IMAGE_TYPE;
	r.mLocalFilePath = QString::fromStdString(fs::path(full_path.toStdString()).lexically_normal().string());
	QImageReader imgReader(r.mLocalFilePath);
	QSize		 size = imgReader.size();
	r.mWidth		  = size.width();
	r.mHeight		  = size.height();
	return r;
}

Resource Resource::fromQuery(const Resource::Id& id) {
	Resource r;
	if (id.mValue < 1) return r;
	r.query(id);
	return r;
}

Resource::Resource()
  : mType(ERROR_TYPE)
  , mDuration(0)
  , mWidth(0)
  , mHeight(0)
  , mThumbnailId(0)
  , mParentId(0)
  , mParentIndex(0) {}

Resource::Resource(const Resource::Id& dbId, const int type)
  : mDbId(dbId)
  , mType(type)
  , mDuration(0)
  , mWidth(0)
  , mHeight(0)
  , mThumbnailId(0)
  , mParentId(0)
  , mParentIndex(0) {}
Resource::Resource(const QString& fullPath)
  : mDbId(0)
  , mType(parseTypeFromFilename(fullPath))
  , mDuration(0)
  , mWidth(0)
  , mHeight(0)
  , mLocalFilePath("")
  , mThumbnailId(0)
  , mParentId(0)
  , mParentIndex(0) {
	setLocalFilePath(fullPath);
}

Resource::Resource(const QString& fullPath, const int type)
  : mDbId(0)
  , mType(type)
  , mDuration(0)
  , mWidth(0)
  , mHeight(0)
  , mLocalFilePath("")
  , mThumbnailId(0)
  , mParentId(0)
  , mParentIndex(0) {
	setLocalFilePath(fullPath);
}

Resource::Resource(const QString& localFullPath, const float width, const float height)
  : mDbId(0)
  , mType(parseTypeFromFilename(localFullPath))
  , mDuration(0)
  , mWidth(width)
  , mHeight(height)
  , mLocalFilePath("")
  , mThumbnailId(0)
  , mParentId(0)
  , mParentIndex(0) {
	setLocalFilePath(localFullPath);
}

Resource::Resource(const Resource::Id dbid, const int type, const double duration, const float width, const float height,
				   const QString filename, const QString path, const int thumbnailId, const QString fullFilePath)
  : mDbId(dbid)
  , mType(type)
  , mDuration(duration)
  , mWidth(width)
  , mHeight(height)
  , mFileName(filename)
  , mPath(path)
  , mLocalFilePath("")
  , mThumbnailId(thumbnailId)
  , mParentId(0)
  , mParentIndex(0) {
	setLocalFilePath(fullFilePath);
}


bool Resource::operator==(const Resource& o) const {
	if (mLocalFilePath != o.mLocalFilePath) return false;
	return mDbId == o.mDbId && mType == o.mType && mDuration == o.mDuration && mWidth == o.mWidth &&
		   mHeight == o.mHeight && mFileName == o.mFileName && mPath == o.mPath;
}

bool Resource::operator!=(const Resource& o) const {
	return (!(*this == o));
}

const std::wstring& Resource::getTypeName() const {
	if (mType == FONT_TYPE)
		return FONT_NAME_SZ;
	else if (mType == IMAGE_TYPE)
		return IMAGE_NAME_SZ;
	else if (mType == IMAGE_SEQUENCE_TYPE)
		return IMAGE_SEQUENCE_NAME_SZ;
	else if (mType == PDF_TYPE)
		return PDF_NAME_SZ;
	else if (mType == VIDEO_TYPE)
		return VIDEO_NAME_SZ;
	else if (mType == ZIP_TYPE)
		return ZIP_NAME_SZ;
	else if (mType == VIDEO_STREAM_TYPE)
		return VIDEO_STREAM_NAME_SZ;
	else if (mType == WEB_TYPE)
		return WEB_NAME_SZ;
	else if (mType == YOUTUBE_TYPE)
		return YOUTUBE_NAME_SZ;
	return ERROR_NAME_SZ;
}

const QString& Resource::getTypeChar() const {
	if (mType == FONT_TYPE)
		return FONT_TYPE_SZ;
	else if (mType == IMAGE_TYPE)
		return IMAGE_TYPE_SZ;
	else if (mType == IMAGE_SEQUENCE_TYPE)
		return IMAGE_SEQUENCE_TYPE_SZ;
	else if (mType == PDF_TYPE)
		return PDF_TYPE_SZ;
	else if (mType == VIDEO_TYPE)
		return VIDEO_TYPE_SZ;
	else if (mType == ZIP_TYPE)
		return ZIP_TYPE_SZ;
	else if (mType == VIDEO_STREAM_TYPE)
		return VIDEO_STREAM_TYPE_SZ;
	else if (mType == WEB_TYPE)
		return WEB_TYPE_SZ;
	else if (mType == YOUTUBE_TYPE)
		return YOUTUBE_TYPE_SZ;
	return ERROR_TYPE_SZ;
}

void Resource::setLocalFilePath(const QString& localPath, const bool normalizeThePath /*= true*/) {
	if (mType == YOUTUBE_TYPE) {
		// we're assuming the link type here is like https://www.youtube.com/watch?v=GP55q2lBqbY
		// TODO: parse the video url better!
		QString theUrlPrefix = "https://www.youtube.com/watch?v=";
		if (localPath.startsWith(theUrlPrefix)) {
			mLocalFilePath = localPath.mid(theUrlPrefix.size());
			mFileName	   = mLocalFilePath;
		} else {
			// maybe this is already the video id?
			mLocalFilePath = localPath;
			mFileName	   = mLocalFilePath;
		}
	} else if (mType == WEB_TYPE || mType == VIDEO_STREAM_TYPE) {
		mLocalFilePath = localPath;
	} else if (normalizeThePath) {
		mLocalFilePath = QString::fromStdString(dsqt::getNormalizedPath(localPath.toStdString()));
	} else {
		mLocalFilePath = localPath;
	}
}

QString Resource::getAbsoluteFilePath() const {
	if (!mLocalFilePath.isEmpty()) return mLocalFilePath;

	if (mFileName.isEmpty()) return EMPTY_SZ;
	if (mType == WEB_TYPE) return mFileName;
	QDir p(mDbId.getResourcePath());

	if (!p.exists()) return EMPTY_SZ;
	auto path = dsqt::getNormalizedPath(QDir::cleanPath(p.path() + "/" + mPath + "/" + mFileName).toStdString());
	return QString::fromStdString(path);
}

QString Resource::getPortableFilePath() const {
	if (!mLocalFilePath.isEmpty()) {
		return DSEnvironment::contract(mLocalFilePath);
	}

	if (mFileName.isEmpty()) return EMPTY_SZ;
	if (mType == WEB_TYPE) return mFileName;
	auto	   reccyPath = mDbId.getPortableResourcePath();
	QDir	   p(reccyPath);

	if (!p.exists()) {
		return EMPTY_SZ;
	}
	auto path = dsqt::getNormalizedPath(QDir::cleanPath(p.path() + "/" + mPath + "/" + mFileName).toStdString());

	return QString::fromStdString(path);
}

void Resource::clear() {
	mDbId.clear();
	mType	  = ERROR_TYPE;
	mDuration = 0.0;
	mWidth	  = 0.0f;
	mHeight	  = 0.0f;
	mFileName.clear();
	mPath.clear();
	mThumbnailId = 0;
	mLocalFilePath.clear();
}

bool Resource::empty() const {
	if (!mLocalFilePath.isEmpty()) return false;
	if (!mFileName.isEmpty()) return false;
	return mDbId.empty();
}

void Resource::swap(Resource& r) {
	mDbId.swap(r.mDbId);
	std::swap(mType, r.mType);
	std::swap(mDuration, r.mDuration);
	std::swap(mWidth, r.mWidth);
	std::swap(mHeight, r.mHeight);
	mFileName.swap(r.mFileName);
	mPath.swap(r.mPath);
	std::swap(mThumbnailId, r.mThumbnailId);
	mLocalFilePath.swap(r.mLocalFilePath);
}

bool Resource::isLocal() const {
	// parameters mFileName and mPath correspond to their fields in the resources table
	return !mLocalFilePath.isEmpty() && mFileName.isEmpty() && mPath.isEmpty();
}

void Resource::setTypeFromString(const QString& typeChar) {
	mType = makeTypeFromString(typeChar);
}


const int Resource::makeTypeFromString(const QString& typeChar) {
	if (FONT_TYPE_SZ == typeChar)
		return FONT_TYPE;
	else if (IMAGE_TYPE_SZ == typeChar)
		return IMAGE_TYPE;
	else if (IMAGE_SEQUENCE_TYPE_SZ == typeChar)
		return IMAGE_SEQUENCE_TYPE;
	else if (PDF_TYPE_SZ == typeChar)
		return PDF_TYPE;
	else if (VIDEO_TYPE_SZ == typeChar)
		return VIDEO_TYPE;
	else if (VIDEO_STREAM_TYPE_SZ == typeChar)
		return VIDEO_STREAM_TYPE;
	else if (WEB_TYPE_SZ == typeChar)
		return WEB_TYPE;
	else if (ZIP_TYPE_SZ == typeChar)
		return ZIP_TYPE;
	else if (YOUTUBE_TYPE_SZ == typeChar)
		return YOUTUBE_TYPE;
	else if (AUDIO_TYPE_SZ == typeChar)
		return VIDEO_TYPE;
	else
		return ERROR_TYPE;
}

const int Resource::parseTypeFromFilename(const QString& newMedia) {

	// creating a Poco::File from an empty string and performing
	// any checks throws a runtime exception
	if (newMedia.isEmpty()) {
		return dsqt::Resource::ERROR_TYPE;
	}

	if (newMedia.startsWith("https://www.youtube.com/")) {
		return dsqt::Resource::YOUTUBE_TYPE;
	}

	auto html	  = newMedia.endsWith(".html");
	auto htmlEnd  = newMedia.size() - 5;
	if (newMedia.startsWith("http") || newMedia.endsWith(".html") || newMedia.startsWith("ftp://") ||
		newMedia.startsWith("ftps://")) {
		return dsqt::Resource::WEB_TYPE;
	}

	if (newMedia.startsWith("udp") || newMedia.startsWith("rtsp")) {
		return dsqt::Resource::VIDEO_STREAM_TYPE;
	}

	if (!dsqt::safeFileExistsCheck(newMedia.toStdString(), false)) {
		return dsqt::Resource::ERROR_TYPE;
	}

	QFileInfo filey		  = QFileInfo(newMedia);
	auto	  extensionay = filey.suffix();
	extensionay			  = extensionay.toLower();
	if (extensionay.contains("gif") || extensionay.contains("svg")) {
		return dsqt::Resource::WEB_TYPE;
	} else if (extensionay.contains("pdf")) {
		return dsqt::Resource::PDF_TYPE;

	} else if (extensionay.contains("png") || extensionay.contains("jpg") || extensionay.contains("jpeg")) {
		return dsqt::Resource::IMAGE_TYPE;

	} else if (extensionay.contains("ttf") || extensionay.contains("otf")) {
		return dsqt::Resource::FONT_TYPE;
	} else if (extensionay.contains("zip")) {
		return dsqt::Resource::ZIP_TYPE;
	} else if (extensionay.contains("mov") || extensionay.contains("mp4") || extensionay.contains("mp3") ||
			   extensionay.contains("wav") || extensionay.contains("avi") || extensionay.contains("wmv") ||
			   extensionay.contains("flv") || extensionay.contains("m2v") || extensionay.contains("m4v") ||
			   extensionay.contains("mpg") || extensionay.contains("mkv") || extensionay.contains("3gp") ||
			   extensionay.contains("webm") || extensionay.contains("asf") || extensionay.contains("dat") ||
			   extensionay.contains("divx") || extensionay.contains("dv") || extensionay.contains("f4v") ||
			   extensionay.contains("m2ts") || extensionay.contains("mod") || extensionay.contains("mpe") ||
			   extensionay.contains("ogg") || extensionay.contains("ogv") || extensionay.contains("mpeg") ||
			   extensionay.contains("mts") || extensionay.contains("nsv") || extensionay.contains("ogm") ||
			   extensionay.contains("qt") || extensionay.contains("tod") || extensionay.contains("ts") ||
			   extensionay.contains("vob") || extensionay.contains("m4a") || extensionay.contains("mxf")) {
		return dsqt::Resource::VIDEO_TYPE;
	} else {
		return dsqt::Resource::ERROR_TYPE;
	}
}

}  // namespace dsqt

/**
 * ds::resource_id stream printing
 */
std::ostream& operator<<(std::ostream& os, const dsqt::Resource::Id& o) {
	if (o.mType == o.CMS_TYPE)
		os << "cms:";
	else if (o.mType == o.APP_TYPE)
		os << "app:";
	else
		os << "error:";
	return os << o.mValue;
}

std::wostream& operator<<(std::wostream& os, const dsqt::Resource::Id& o) {
	if (o.mType == o.CMS_TYPE)
		os << L"cms:";
	else if (o.mType == o.APP_TYPE)
		os << L"app:";
	else
		os << L"error:";
	return os << o.mValue;
}

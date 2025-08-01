#pragma once

#ifndef DSRESOURCE_H
#define DSRESOURCE_H

#include <QLoggingCategory>
#include <QRectF>

#include <functional>
#include <string>
#include <vector>

Q_DECLARE_LOGGING_CATEGORY(lgResource)
Q_DECLARE_LOGGING_CATEGORY(lgResourceVerbose)

#ifdef QT_SUPPORTS_INT128
typedef qint128 valType__;
#else
typedef qint64 valType__;
#endif

namespace dsqt {
class DataBuffer;
class ResourceList;


/**
 * \class Resource
 * \brief Encapsulate a row in the Resources table.
 */
class DsResource {
  public:
	/**
	 * \struct ds::Resource::id
	 * \brief A single resource ID, composed of a type and value.
	 * Resources can exist in one of several databases, hence the type.
	 */
	struct Id {
		/// The traditional type for remotely-generated user content,
		/// but also the only type supported until this structure was added.
		///  All legacy projects will have this type and only this type.
		static const char CMS_TYPE = 0;
		/// Local application resources like UI components.
		static const char APP_TYPE = 1;
		/// Custom database types can be this value or less.
		static const char CUSTOM_TYPE = -32;

		/// All framework-defined types will be positive.  Negative values
		/// are reserved for applications to create fake resource_ids.
		char mType;
		valType__ mValue;

		Id();
		Id(const valType__ value);
		Id(const QString value);
		Id(const char type, const valType__ value);
		Id(const char type, const QString value);

		bool operator==(const Id&) const;
		bool operator!=(const Id&) const;
		bool operator>(const Id&) const;
		bool operator<(const Id&) const;
		/// Comparison with a raw value
		bool operator>(const valType__ value) const;
		bool operator>=(const valType__ value) const;
		bool operator<(const valType__ value) const;
		bool operator<=(const valType__ value) const;

		bool operator>(const QString value) const;
		bool operator>=(const QString value) const;
		bool operator<(const QString value) const;
		bool operator<=(const QString value) const;

		bool empty() const;
		void clear();
		void swap(Id&);

		/// Assumes the string is in my string output format (type:value)
		bool tryParse(const QString&);

		/// Get the path to the resources for this type, or directly to the
		/// database (which will always be in the resource path).
		const QString& getResourcePath() const;
		const QString& getDatabasePath() const;
		/// These are used when transporting a resource across a network.
		/// They can be resolved to correct paths with ds::Environment::expand()
		const QString& getPortableResourcePath() const;

		/// Utility to report and log an error if I'm missing path information
		bool verifyPaths() const;

		void writeTo(DataBuffer&) const;
		bool readFrom(DataBuffer&);

		/// The engines are required to set paths to the various resource database before
		/// anyone does anything.  This assumes the traditional CMS path -- a resource
		/// location and a database file inside it.  From this info, the legacy and CMS
		/// (which are currently the same thing) paths are set, and the files are probed
		/// to set any additional paths.
		/// root and have the newer db structure figured automatically.
		/// OK!  resource and db are now considered legacy values.  Project path will
		/// have a segment of the path to the resources -- it won't have the start,
		/// which is standardized to `My Documents\downstream\resources`, and it won't have
		/// the end, which is now a common structure.  Currently this only applies to
		/// app-local resources, the traditional CMS resources have the traditional CMS setup.
		static void setupPaths(const QString& resource, const QString& db, const QString& projectPath);
		static void setupPaths(const std::string& resource, const std::string& db, const std::string& projectPath);
		/// The client app can set a function responsible for returning the paths to any
		/// custom database types.
		/// NOTE:  For efficiency, always return a valid string ref, even if it's on an empty string.
		/// Never return a string newly constructed in the function.
		static void setupCustomPaths(const std::function<const QString&(const DsResource::Id&)>& resourcePath,
									 const std::function<const QString&(const DsResource::Id&)>& dbPath);
	};

  public:
	static const int ERROR_TYPE			  = 0;
	static const int FONT_TYPE			  = 1;
	static const int IMAGE_TYPE			  = 2;
	static const int IMAGE_SEQUENCE_TYPE  = 3;
	static const int PDF_TYPE			  = 4;
	static const int VIDEO_TYPE			  = 5;
	static const int WEB_TYPE			  = 6;
	static const int VIDEO_STREAM_TYPE	  = 7;
	static const int ZIP_TYPE			  = 8;
	static const int VIDEO_PANORAMIC_TYPE = 9;
	static const int YOUTUBE_TYPE		  = 10;

  public:
	/// Mainly for debugging
	static DsResource fromImage(const QString& full_path);


	DsResource();
	DsResource(const DsResource::Id& dbId, const int type);

	/// Sets the absolute filepath, type is auto-detected, no other parameters are filled out
	DsResource(const QString& localFullPath);
	/// Sets the absolute filepath, no other parameters are filled out
	DsResource(const QString& localFullPath, const int type);
	/// Sets the absolute filepath, type is auto-detected. This is intended for streams
	DsResource(const QString& localFullPath, const float width, const float height);

	/// In case you have this queried/constructed already
	DsResource(const DsResource::Id dbid, const int type, const double duration, const float width, const float height,
			 const QString filename, const QString path, const int thumbnailId, const QString fullFilePath);

	bool operator==(const DsResource&) const;
	bool operator!=(const DsResource&) const;

	const DsResource::Id& getDbId() const { return mDbId; }
	void				setDbId(const DsResource::Id& dbId) { mDbId = dbId; }

	const std::wstring& getTypeName() const;
	const QString&		getTypeChar() const;
	int					getType() const { return mType; }
	void				setType(const int newType) { mType = newType; }

	double getDuration() const { return mDuration; }
	void   setDuration(const float newDur) { mDuration = newDur; }

	float getWidth() const { return mWidth; }
	void  setWidth(const float newWidth) { mWidth = newWidth; }

	float getHeight() const { return mHeight; }
	void  setHeight(const float newHeight) { mHeight = newHeight; }

	QRectF getCrop() const {
		return QRectF(mCropX, mCropY, mCropW, mCropH);
	}
	void setCrop(const QRectF cropRect) {
		// ci::Rectf myRect = ci::Rectf(1.f, 2.f, 3.f, 4.f);
		mCropX = cropRect.x();
		mCropY = cropRect.y();
		mCropW = cropRect.width();
		mCropH = cropRect.height();
	}
	void setCrop(const float cropX, const float cropY, const float cropW, const float cropH) {
		mCropX = cropX;
		mCropY = cropY;
		mCropW = cropW;
		mCropH = cropH;
	}

	int	 getThumbnailId() const { return mThumbnailId; }
	void setThumbnailId(const int thub) { mThumbnailId = thub; }

	/// If this resource has a parent (like pages of a PDF), get the ID for the parent
	int	 getParentId() const { return mParentId; }
	void setParentId(const int parentId) { mParentId = parentId; }

	/// The sort order of this resource in it's parent
	int	 getParentIndex() const { return mParentIndex; }
	void setParentIndex(const int parentIndx) { mParentIndex = parentIndx; }

	std::vector<DsResource>& getChildrenResources() { return mChildrenResources; }
	void setChildrenResources(const std::vector<DsResource>& newChildren) { mChildrenResources = newChildren; }

	/// If you want to simply store a path to a thumbnail
	///	This is NOT filled out by default in the query() methods, you need to supply this yourself
	QString getThumbnailFilePath() const {
		return mThumbnailFilePath;
	}
	void setThumbnailFilePath(const QString& thumbPath) {
		mThumbnailFilePath = thumbPath;
	}

	/// Answer the full path to my file
	QString getAbsoluteFilePath() const;

	/// Local file path is the path to a file, generally not tracked by a database. This will be used instead of
	/// resource ID, and FileName and Path won't be used.
	void setLocalFilePath(const QString& localPath, const bool normalizeThePath = true);

	/// Answer an abstract file path that can be resolved to an absolute one via ds::Environment::expand().
	QString getPortableFilePath() const;

	QString getFileName() const {
		return mFileName;
	}
	void setFileName(const QString& fileName) {
		mFileName = fileName;
	}

	/// Clears the currently set info
	void clear();

	/// If anything has been set
	bool empty() const;
	void swap(DsResource&);

	/// Expects a single-character type (v, i, p, w, f, s)
	void setTypeFromString(const QString& typeChar);
	/// Return the int value for the string type
	static const int makeTypeFromString(const QString& typeChar);

	/// Returns the type parsed from the filename, primarily using the file extension.
	/// Creates an error type if it's a file type (not web type) and the file doesn't exist
	/// Use the full file path or web URL, not a single character like above
	static const int parseTypeFromFilename(const QString& fileName);

	/// Answers true if ds::DsResource was constructed from a local
	/// file instead of an actual element in db. via fromImage method for example.
	bool isLocal() const;

	/// Query the database set as the resources database for my contents. Obviously, this is also an expensive
	/// operation.
	/// I don't think we need these any longer.
	// bool query(const DsResource::Id&);

	/// The argument is the full thumbnail, if you want it.
	// bool query(const DsResource::Id&, DsResource* outThumb);

  private:
	friend class ResourceList;

	DsResource::Id mDbId;

	/// See the public types above
	int	   mType;
	double mDuration;
	float  mWidth, mHeight;
	float  mCropX = 0.f;
	float  mCropY = 0.f;
	float  mCropW = 1.f;
	float  mCropH = 1.f;

	/// filename and path are applied when querying from a database
	QString mFileName;
	QString mPath;

	/// Overrides FileName and Path
	QString mLocalFilePath;

	/// thumbnail id used when querying from a database
	int mThumbnailId;
	/// can be set manually as a convenience
	QString mThumbnailFilePath;

	/// Some resources are representations of another resource (like pages of a pdf), this lets you link the two
	int					  mParentId;
	int					  mParentIndex;
	std::vector<DsResource> mChildrenResources;
};

}  // namespace dsqt

// Make the resource ID available to standard stream operators
std::ostream&  operator<<(std::ostream&, const dsqt::DsResource::Id&);
std::wostream& operator<<(std::wostream&, const dsqt::DsResource::Id&);

/*\cond Have doxygen ignore this, since it's an internal function that pops the std namespace on the main list
		 Make the resource ID available for hashing functions
*/
namespace std {
/*template<>
struct hashy<ds::Resource::Id> : public unary_function < ds::Resource::Id, size_t > {
	size_t operator()(const ds::Resource::Id& id) const {
		return id.mType + (id.mValue << 8);
	}
};*/

template <>
struct hash<dsqt::DsResource::Id> {
	typedef size_t result_type;
	size_t		   operator()(const dsqt::DsResource::Id& id) const {
		return (id.mType & 0xff) + ((static_cast<size_t>(id.mValue) & 0xffffffff) << 8);
	}
};

/* \endcond */
} // namespace std

#endif // DS_DATA_RESOURCE_H_

#pragma once
#ifndef DS_UTIL_IMAGEMETADATA_H_
#define DS_UTIL_IMAGEMETADATA_H_

#include <QLoggingCategory>
#include <QObject>
#include <filesystem>
#include <glm/glm.hpp>
#include <string>
#include "utility/url_image_loader.h"

Q_DECLARE_LOGGING_CATEGORY(lgImageMetaData)
namespace fs = std::filesystem;
namespace dsqt {

/**
 * \class ImageMetaData
 * \brief Read meta data for image files.
 * subclass of QObject
 * NOTE: This can be VERY slow, if the image needs to be loaded.
 */
class ImageMetaData : public QObject {
	Q_OBJECT
  public:
	ImageMetaData(QObject* parent = nullptr);
	ImageMetaData(const std::string& filename);

	/// Clears any stored w/h info
	static void clearMetadataCache();

	bool empty() const;
	void add(const std::string& filePath, const glm::vec2 size);

	glm::vec2 mSize;
	UrlImageLoader mLoader;

  private:
};

}  // namespace dsqt

#endif // DS_UTIL_IMAGEMETADATA_H_

#pragma once
#ifndef DSFILEMETADATA_H_
#define DSFILEMETADATA_H_

#include "utility/dsStringUtils.h"

#include <QLoggingCategory>

#include <filesystem>
#include <string>
#include <vector>

Q_DECLARE_LOGGING_CATEGORY(lgFileMetaData)
Q_DECLARE_LOGGING_CATEGORY(lgFileMetaDataVerbose)
namespace fs = std::filesystem;
namespace dsqt {

/// Gets the absolute file path given a base file path a relative path to that base.
std::string filePathRelativeTo(const std::string& base, const std::string& relative);

/// Poco throws an exception when calling file.exists() and the file doesn't exist, so this handles that for you with no
/// exceptions thrown.
bool safeFileExistsCheck(const std::string filePath, const bool allowDirectory = true);

/// Return a platform-native normalized path string
std::string getNormalizedPath(const std::string& path);
std::string getNormalizedPath(const fs::path& path);


/**
 * \class FileMetaData
 * Collection of file meta data.
 */
class DsFileMetaData {
  public:
    DsFileMetaData();
    explicit DsFileMetaData(const std::string& filename);

	void parse(const std::string& filename);

	size_t			   keyCount() const;
	bool			   contains(const std::string& key) const;
	const std::string& keyAt(const size_t index) const;
	const std::string& valueAt(const size_t index) const;
	const std::string& findValue(const std::string& key) const;
	template <typename T>
	T findValueType(const std::string& key, const T error) const;

  private:
	std::vector<std::pair<std::string, std::string>> mAttrib;
};

template <typename T>
T DsFileMetaData::findValueType(const std::string& key, const T error) const {
	const std::string& v = findValue(key);
	if (v.empty()) return error;
	T ans;
	if (!dsqt::string_to_value(v, ans)) return error;
	return ans;
}

}  // namespace dsqt

#endif // DS_UTIL_FILEMETADATA_H_

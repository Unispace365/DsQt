
#include "file_meta_data.h"


#include <core/dsenvironment.h>
#include <utility/string_util.h>

Q_LOGGING_CATEGORY(lgFileMetaData, "file_meta_data")
Q_LOGGING_CATEGORY(lgFileMetaDataVerbose, "file_meta_data.verbose")
namespace dsqt {

namespace {
	const std::string EMPTY_SZ("");
}

/**
 * \class FileMetaData
 */
FileMetaData::FileMetaData() {}

FileMetaData::FileMetaData(const std::string& filename) {
	parse(filename);
}

void FileMetaData::parse(const std::string& filename) {
	mAttrib.clear();

	fs::path p(filename);

	std::vector<std::string> splitString = dsqt::split(p.filename().string(), ".");
	for (auto it = splitString.begin(), it2 = splitString.end(); it != it2; ++it) {
		// First item is base name
		if (it == splitString.begin()) continue;
		std::string	 str = *it;
		const size_t pos = str.find("_");
		if (pos == std::string::npos) continue;

		std::vector<std::string> meta = split(str, "_");
		if (meta.size() != 2) continue;
		mAttrib.push_back(std::pair<std::string, std::string>(meta.front(), meta.back()));
	}
}

size_t FileMetaData::keyCount() const {
	return mAttrib.size();
}

bool FileMetaData::contains(const std::string& key) const {
	for (auto it = mAttrib.begin(), end = mAttrib.end(); it != end; ++it) {
		if (it->first == key) return true;
	}
	return false;
}

const std::string& FileMetaData::keyAt(const size_t index) const {
	if (index >= mAttrib.size()) return EMPTY_SZ;
	return mAttrib[index].first;
}

const std::string& FileMetaData::valueAt(const size_t index) const {
	if (index >= mAttrib.size()) return EMPTY_SZ;
	return mAttrib[index].second;
}

const std::string& FileMetaData::findValue(const std::string& key) const {
	for (auto it = mAttrib.begin(), end = mAttrib.end(); it != end; ++it) {
		if (it->first == key) return it->second;
	}
	return EMPTY_SZ;
}


bool safeFileExistsCheck(const std::string filePath, const bool allowDirectory) {
	if (filePath.empty()) return false;

	fs::path   xmlFile(filePath);
	bool	   fileExists = false;
	try {
		if (fs::exists(xmlFile)) {
			fileExists = true;
		}
		if (fileExists && !allowDirectory) {
			if (fs::is_directory(xmlFile)) fileExists = false;
		}
	} catch (std::exception&) {}

	return fileExists;
}

std::string filePathRelativeTo(const std::string& base, const std::string& relative) {
	if (relative.find("%APP%") != std::string::npos || relative.find("%LOCAL%") != std::string::npos) {
		return DSEnvironment::expand(relative);
	}

	using namespace std::filesystem;
	std::error_code e;
	auto			cpath = current_path();
	if (!base.empty()) current_path(path(base).parent_path());
	std::string ret = std::filesystem::canonical(path(relative), e).string();
	current_path(cpath);
	if (e) {
		qCDebug(lgFileMetaData) << "Trying to use bad relative file path: " << relative << ": " << e.message();
	}
	return ret;
}

std::string getNormalizedPath(const std::string& path) {
	auto ret = path;
	dsqt::replace(ret, "\\", "/");

	ret = fs::path(ret).string();

	return ret;
}

std::string getNormalizedPath(const fs::path& path) {
	return getNormalizedPath(path.string());
}

}  // namespace dsqt

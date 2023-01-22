#include "dssettings.h"
#include <toml++/toml.h>
#include <filesystem>
#include <iostream>
#include <string>
#include "core/dsenvironment.h"

Q_LOGGING_CATEGORY(settingsParser, "settings.parser")
Q_LOGGING_CATEGORY(settingsParserWarn, "settings.parser.warning")
namespace dsqt {
struct GeomElements;

std::string									   DSSettings::mConfigurationDirectory = "";
bool										   DSSettings::mLoadedConfiguration	   = false;
std::unordered_map<std::string, DSSettingsRef> DSSettings::sSettings;

DSSettings::DSSettings(std::string name, QObject* parent) : QObject(parent) {
	mName = name;
}

DSSettings::~DSSettings() {}

bool DSSettings::loadSettingFile(const std::string& file) {

	std::string fullFile = DSEnvironment::expand(file);
	if (!std::filesystem::exists(fullFile)) {
		qCWarning(settingsParserWarn) << "File doesn't exist warning: Attempting to load file \"" << fullFile.c_str()
									  << "\" but it does not exist";
		return false;
	}
	auto		 clearItr	= mResultStack.end();
	SettingFile* loadResult = nullptr;
	for (auto resultItr = mResultStack.begin(); resultItr != mResultStack.end(); ++resultItr) {
		if (resultItr->filepath == fullFile) {
			qCWarning(settingsParserWarn) << "File already loaded warning: Updating already loaded file";
			loadResult = &(*resultItr);
		}
	}

	if (!loadResult) {
		mResultStack.push_back(SettingFile());
		loadResult = &mResultStack.back();
	}

	loadResult->filepath = fullFile;
	loadResult->valid	 = false;
	try {
		loadResult->data  = toml::parse_file(fullFile);
		loadResult->valid = true;
	} catch (const toml::parse_error& e) {
		qCWarning(settingsParserWarn) << "Faild to parse setting file \"" << fullFile.c_str() << "\n:" << e.what();
		return false;
	}

	return true;
}


std::tuple<bool, DSSettingsRef> DSSettings::getSettingsOrCreate(const std::string& name, QObject* parent) {

	if (sSettings.find(name) != sSettings.end()) {
		qCDebug(settingsParser) << "getSettingsOrCreate: Found Setting " << QString::fromStdString(name);
		return {true, sSettings[name]};
	}

	DSSettingsRef retVal = DSSettingsRef(new DSSettings(name, parent));
	sSettings[name]		 = retVal;
	return {false, retVal};
}

DSSettingsRef DSSettings::getSettings(const std::string& name) {

	if (sSettings.find(name) == sSettings.end()) {
		return DSSettingsRef(nullptr);
	}

	return sSettings[name];
}

bool DSSettings::forgetSettings(const std::string& name) {
	auto iter = sSettings.find(name);
	if (iter != sSettings.end()) {
		sSettings.erase(iter);
		return true;
	}
	return false;
}

// template<class T> std::optional<T> DSSettings::getValue(const std::string& key){
//      auto retval = getWithMeta<T>(key);
//      if(retval){
//         auto [value,x,y] = retval.value();
//         return value;
//      }
//      return std::optional<T>();
// }

// template<> std::optional<std::string> DSSettings::getValue(const std::string& key){
//      auto retval = getWithMeta<std::string>(key);
//      if(retval){
//         auto [value,x,y] = retval.value();
//         return value;
//      }
//      return std::optional<std::string>();
// }


std::optional<NodeWMeta> DSSettings::getNodeViewWithMeta(const std::string& key) {
	std::string returnPath = "";

	if (mRuntimeResult.data.contains(key)) {
		mRuntimeResult.filepath	  = "runtime";
		returnPath				  = mRuntimeResult.filepath;
		toml::table*	metaTable = nullptr;
		toml::node_view value	  = toml::node_view<toml::node>();
		const auto&		nodeView  = mRuntimeResult.data.at_path(key);

		if (nodeView.is_array() && nodeView.as_array()->size() > 0) {
			value = toml::node_view<toml::node>(nodeView.as_array()->at(0));
		}
		if (nodeView.is_array() && nodeView.as_array()->size() > 1 && nodeView.as_array()->at(1).is_table()) {
			metaTable = nodeView.as_array()->at(1).as_table();
		}
		return std::optional(NodeWMeta({value, metaTable, returnPath}));
	}

	for (auto iter = mResultStack.rbegin(); iter != mResultStack.rend(); ++iter) {
		auto& stackItem = *iter;
		//        if(key==std::string("config_folder")){
		//            std::stringstream ss;
		//            ss << stackItem.data;
		//            qDebug() << ss.str().c_str();
		//        }
		if (stackItem.data.at_path(key)) {
			returnPath				  = stackItem.filepath;
			toml::table*	metaTable = nullptr;
			toml::node_view value	  = toml::node_view<toml::node>();
			const auto&		nodeView  = stackItem.data.at_path(key);
			value					  = nodeView;
			if (nodeView.is_array() && nodeView.as_array()->size() > 0) {
				value = toml::node_view<toml::node>(nodeView.as_array()->at(0));
			}
			if (nodeView.is_array() && nodeView.as_array()->size() > 1 && nodeView.as_array()->at(1).is_table()) {
				metaTable = nodeView.as_array()->at(1).as_table();
			}
			return std::optional(NodeWMeta({value, metaTable, returnPath}));
		}
	}
	qCDebug(settingsParser) << "Did not find setting for key \"" << key.c_str() << "\"";
	return std::nullopt;  //{toml::node_view<toml::node>(),nullptr,""};
}


template <typename T>
void DSSettings::set(std::string& key, T& value) {
	toml::path p(key);
	// build any missing parts
	auto iter = p.begin();
	for (iter = p.begin(); iter != p.end(); ++iter) {
		if (true) {}
	}
}


}  // namespace dsqt

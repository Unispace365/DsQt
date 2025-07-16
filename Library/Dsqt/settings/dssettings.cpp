#include "dssettings.h"
#include <toml++/toml.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <qtimezone.h>
#include "core/dsenvironment.h"

Q_LOGGING_CATEGORY(lgSettingsParser, "settings.parser")
Q_LOGGING_CATEGORY(lgSPVerbose, "settings.parser.verbose")

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
		qCWarning(lgSPVerbose) << "File doesn't exist warning: Attempting to load file \"" << fullFile.c_str()
							   << "\" but it does not exist";
		return false;
	}
	auto		 clearItr	= mResultStack.end();
	SettingFile* loadResult = nullptr;
	for (auto resultItr = mResultStack.begin(); resultItr != mResultStack.end(); ++resultItr) {
		if (resultItr->filepath == fullFile) {
			qCWarning(lgSPVerbose) << "File already loaded warning: Updating already loaded file";
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
		qCWarning(lgSettingsParser) << "Faild to parse setting file \"" << fullFile.c_str() << "\n:" << e.what();
		return false;
	}

	return true;
}


std::tuple<bool, DSSettingsRef> DSSettings::getSettingsOrCreate(const std::string& name, QObject* parent) {

	if (sSettings.find(name) != sSettings.end()) {
		qCDebug(lgSPVerbose) << "getSettingsOrCreate: Found Setting " << QString::fromStdString(name);
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

toml::node* DSSettings::getRawNode(const std::string& key,bool onlyBase) {
    if (onlyBase){
        auto baseResult = mResultStack[0].data.at_path(key);
        if(baseResult){
            return baseResult.node();
        }
        return nullptr;
    }
    if (mRuntimeResult.data.at_path(key)) {
		return mRuntimeResult.data.at_path(key).node();
	}

	for (auto iter = mResultStack.rbegin(); iter != mResultStack.rend(); ++iter) {
		auto& stackItem = *iter;
		if (stackItem.data.at_path(key)) {
			return stackItem.data.at_path(key).node();
		}
	}
	return nullptr;
}

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
	qCDebug(lgSettingsParser) << "Did not find setting for key \"" << key.c_str() << "\"";
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



//-----------------------------------------------------------------------

// Convert a toml::node_view<const toml::node> to QVariant
QVariant DSSettings::tomlNodeViewToQVariant(const toml::node_view<const toml::node>& n)
{
    // Boolean?
    if (n.is_boolean())
    {
        // n.value<bool>() returns std::optional<bool>
        std::optional<bool> optBool = n.value<bool>();
        if (optBool.has_value())
            return QVariant(optBool.value());
        return QVariant();
    }
    // Integer?
    else if (n.is_integer())
    {
        std::optional<toml::int64_t> optInt = n.value<toml::int64_t>();
        if (optInt.has_value())
            return QVariant(static_cast<qint64>(optInt.value()));
        return QVariant();
    }
    // Floating-point?
    else if (n.is_floating_point())
    {
        std::optional<double> optDouble = n.value<double>();
        if (optDouble.has_value())
            return QVariant(optDouble.value());
        return QVariant();
    }
    // String?
    else if (n.is_string())
    {
        std::optional<std::string> optStr = n.value<std::string>();
        if (optStr.has_value())
            return QVariant(QString::fromStdString(optStr.value()));
        return QVariant();
    }
    // Date-time?
    else if (n.is_date_time())
    {
        std::optional<toml::date_time> optDT = n.value<toml::date_time>();
        if (optDT.has_value())
        {
            toml::date_time dt = optDT.value();
            QDate qd(dt.date.year, dt.date.month, dt.date.day);
            QTime qt(dt.time.hour, dt.time.minute, dt.time.second);
            QDateTime qdt(qd, qt, dt.offset.has_value() ? QTimeZone(dt.offset->minutes*60) : QTimeZone(QTimeZone::Initialization::LocalTime));
            return QVariant::fromValue(qdt);
        }
        return QVariant();
    }
    // Array?
    else if (n.is_array())
    {
        if (auto arrPtr = n.as_array())
        {
            // Construct a node_view over the array, then recurse
            return QVariant::fromValue(
                tomlArrayViewToVariantList(toml::node_view<const toml::node>(*arrPtr))
                );
        }
        return QVariant();
    }
    // Table?
    else if (n.is_table())
    {
        if (auto tabPtr = n.as_table())
        {
            // Construct a node_view over the table, then recurse
            return QVariant::fromValue(
                tomlTableViewToVariantMap(toml::node_view<const toml::node>(*tabPtr))
                );
        }
        return QVariant();
    }

    // Fallback: unrecognized type → invalid QVariant
    return QVariant();
}

// Recursively convert a toml::node_view<const toml::array> into QVariantList
QVariantList DSSettings::tomlArrayViewToVariantList(const toml::node_view<const toml::node>& arrView)
{
    QVariantList list;
    //list.reserve(static_cast<int>(arrView->size()));

    for (auto const& element : *arrView.as_array())
    {
        // For each element (a toml::node), create a node_view and recurse
        list.append(
            tomlNodeViewToQVariant(toml::node_view<const toml::node>(element))
            );
    }
    return list;
}

// Recursively convert a toml::node_view<const toml::table> into QVariantMap
QVariantMap DSSettings::tomlTableViewToVariantMap(const toml::node_view<const toml::node>& tabView)
{
    QVariantMap map;
    //map.reserve(static_cast<int>(tabView->size()));

    for (auto const& [key, nodeRef] : *tabView.as_table())
    {
        // For each key/value, wrap the node in a node_view and recurse
        map.insert(
            QString::fromStdString(std::string(key.str())),
            tomlNodeViewToQVariant(toml::node_view<const toml::node>(nodeRef))
            );
    }
    return map;
}

// Example usage:
//
// toml::parse_result pr = toml::parse_file("config.toml");
// if (!pr) { /* handle parse errors here */ }
// toml::table const& rootTbl = *pr;
//
// // Suppose there is a top-level array named "myArray":
// if (auto arrNodePtr = rootTbl["myArray"].as_array())
// {
//     // Create a node_view over that array pointer
//     toml::node_view<const toml::array> arrView(*arrNodePtr);
//     QVariantList qlist = tomlArrayViewToVariantList(arrView);
//     // Now 'qlist' can be used anywhere a QVariantList is expected.
// }
//






}  // namespace dsqt

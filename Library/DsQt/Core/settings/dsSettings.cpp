

#include "settings/dsSettings.h"
#include "core/dsEnvironment.h"

#include <QFile>
#include <QUrl>
#include <filesystem>
#include <qtimezone.h>
#include <string>

#if defined(DSQT_USE_TOML)
#include <toml++/toml.h>
#endif

Q_LOGGING_CATEGORY(lgSettingsParser, "settings.parser")
Q_LOGGING_CATEGORY(lgSPVerbose, "settings.parser.verbose")

namespace dsqt {
struct GeomElements;

std::string                                    DsSettings::mConfigurationDirectory = "";
bool                                           DsSettings::mLoadedConfiguration    = false;
std::unordered_map<std::string, DsSettingsRef> DsSettings::sSettings;

DsSettings::DsSettings(std::string name, QObject* parent)
    : QObject(parent) {
    mName = name;
}

DsSettings::~DsSettings() {
}


bool DsSettings::loadSettingFileFromResource(const QString& file) {
    return loadSettingFileFromResource(file.toStdString());
}

bool DsSettings::loadSettingFileFromResource(const std::string& file) {

    QFile fileObj(QString::fromStdString(file));
    if (!fileObj.exists()) {
        qCWarning(lgSPVerbose) << "Resource file doesn't exist warning: Attempting to load resource file \"" << file
                               << "\" but it does not exist";
        return false;
    }

    auto         clearItr   = mResultStack.end();
    SettingFile* loadResult = nullptr;

    // check if we loaded this path already
    for (auto resultItr = mResultStack.begin(); resultItr != mResultStack.end(); ++resultItr) {
        if (resultItr->filepath == file) {
            qCWarning(lgSPVerbose) << "File already loaded warning: Updating already loaded file";
            loadResult = &(*resultItr);
        }
    }

    // if not push a new result on the stack
    if (!loadResult) {
        mResultStack.push_back(SettingFile());
        loadResult = &mResultStack.back();
    }

    // either way fill the result with the new file.
    loadResult->filepath = file;
    loadResult->valid    = false;


    try {
        if (fileObj.open(QIODevice::ReadOnly | QIODevice::Text)) {
            auto text = fileObj.readAll().toStdString();
            fileObj.close();
            loadResult->data  = toml::parse(text);
            loadResult->valid = true;
        }
    } catch (const toml::parse_error& e) {
        qCWarning(lgSettingsParser) << "Failed to parse setting resource \"" << file.c_str() << "\n:" << e.what();
        return false;
    }

    return true;
}

bool DsSettings::loadSettingFile(const std::string& file) {

    std::string fullFile = DsEnvironment::expand(file);
    if (!std::filesystem::exists(fullFile)) {
        qCWarning(lgSPVerbose) << "File doesn't exist warning: Attempting to load file \"" << fullFile.c_str()
                               << "\" but it does not exist";
        return false;
    }
    auto         clearItr   = mResultStack.end();
    SettingFile* loadResult = nullptr;

    // check if we loaded this path already
    for (auto resultItr = mResultStack.begin(); resultItr != mResultStack.end(); ++resultItr) {
        if (resultItr->filepath == fullFile) {
            qCWarning(lgSPVerbose) << "File already loaded warning: Updating already loaded file";
            loadResult = &(*resultItr);
        }
    }

    // if not push a new result on the stack
    if (!loadResult) {
        mResultStack.push_back(SettingFile());
        loadResult = &mResultStack.back();
    }


    //either way fill the result with the new file.
	loadResult->filepath = fullFile;
	loadResult->valid	 = false;
	try {
		loadResult->data  = toml::parse_file(fullFile);
		loadResult->valid = true;
	} catch (const toml::parse_error& e) {
        qCWarning(lgSettingsParser) << "Failed to parse setting file \"" << fullFile.c_str() << "\n:" << e.what();
		return false;
    } catch (const std::exception& e) {
        qCWarning(lgSettingsParser) << "Failed to open setting file \"" << fullFile.c_str() << "\n:" << e.what();
        return false;
    }

    return true;
}


std::tuple<bool, DsSettingsRef> DsSettings::getSettingsOrCreate(const std::string& name, QObject* parent) {

    if (sSettings.find(name) != sSettings.end()) {
        qCDebug(lgSPVerbose) << "getSettingsOrCreate: Found Setting " << QString::fromStdString(name);
        return {true, sSettings[name]};
    }

    DsSettingsRef retVal = DsSettingsRef(new DsSettings(name, parent));
    sSettings[name]      = retVal;
    return {false, retVal};
}

DsSettingsRef DsSettings::getSettings(const std::string& name) {

    if (sSettings.find(name) == sSettings.end()) {
        return {};
    }

    return sSettings[name];
}

bool DsSettings::forgetSettings(const std::string& name) {
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

toml::node* DsSettings::getRawNode(const std::string& key, bool onlyBase) {
    if (onlyBase) {
        auto baseResult = mResultStack[0].data.at_path(key);
        if (baseResult) {
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

std::vector<std::pair<std::string, std::optional<NodeWMeta>>>
DsSettings::getNodeViewStackWithMeta(const std::string& key) {
    std::string returnPath = "";
    auto        stack      = std::vector<std::pair<std::string, std::optional<NodeWMeta>>>();
    for (auto iter = mResultStack.begin(); iter != mResultStack.end(); ++iter) {
        auto& stackItem = *iter;
        //        if(key==std::string("config_folder")){
        //            std::stringstream ss;
        //            ss << stackItem.data;
        //            qDebug() << ss.str().c_str();
        //        }
        if (stackItem.data.at_path(key)) {
            returnPath                = stackItem.filepath;
            toml::table*    metaTable = nullptr;
            toml::node_view value     = toml::node_view<toml::node>();
            const auto&     nodeView  = stackItem.data.at_path(key);
            value                     = nodeView;
            if (nodeView.is_array() && nodeView.as_array()->size() > 0) {
                value = toml::node_view<toml::node>(nodeView.as_array()->at(0));
            }
            if (nodeView.is_array() && nodeView.as_array()->size() > 1 && nodeView.as_array()->at(1).is_table()) {
                metaTable = nodeView.as_array()->at(1).as_table();
            }
            stack.push_back(std::make_pair(returnPath, std::optional(NodeWMeta({value, metaTable, returnPath}))));
        }
    }

    if (mRuntimeResult.data.contains(key)) {
        mRuntimeResult.filepath   = "runtime";
        returnPath                = mRuntimeResult.filepath;
        toml::table*    metaTable = nullptr;
        toml::node_view value     = toml::node_view<toml::node>();
        const auto&     nodeView  = mRuntimeResult.data.at_path(key);

        if (nodeView.is_array() && nodeView.as_array()->size() > 0) {
            value = toml::node_view<toml::node>(nodeView.as_array()->at(0));
        }
        if (nodeView.is_array() && nodeView.as_array()->size() > 1 && nodeView.as_array()->at(1).is_table()) {
            metaTable = nodeView.as_array()->at(1).as_table();
        }
        stack.push_back(std::make_pair(returnPath, std::optional(NodeWMeta({value, metaTable, returnPath}))));
    }
    if (stack.empty()) {
        qCDebug(lgSPVerbose) << "Did not find any settings for key \"" << key.c_str() << "\"";
    }

    return stack;
}

std::optional<NodeWMeta> DsSettings::getNodeViewWithMeta(const std::string& key) {
    std::string returnPath = "";

    if (mRuntimeResult.data.contains(key)) {
        mRuntimeResult.filepath   = "runtime";
        returnPath                = mRuntimeResult.filepath;
        toml::table*    metaTable = nullptr;
        toml::node_view value     = toml::node_view<toml::node>();
        const auto&     nodeView  = mRuntimeResult.data.at_path(key);

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
            returnPath                = stackItem.filepath;
            toml::table*    metaTable = nullptr;
            toml::node_view value     = toml::node_view<toml::node>();
            const auto&     nodeView  = stackItem.data.at_path(key);
            value                     = nodeView;
            if (nodeView.is_array() && nodeView.as_array()->size() > 0) {
                value = toml::node_view<toml::node>(nodeView.as_array()->at(0));
            }
            if (nodeView.is_array() && nodeView.as_array()->size() > 1 && nodeView.as_array()->at(1).is_table()) {
                metaTable = nodeView.as_array()->at(1).as_table();
            }
            return std::optional(NodeWMeta({value, metaTable, returnPath}));
        }
    }
    qCDebug(lgSPVerbose) << "Did not find setting for key \"" << key.c_str() << "\"";
    return std::nullopt; //{toml::node_view<toml::node>(),nullptr,""};
}


template <typename T>
void DsSettings::set(std::string& key, T& value) {
    toml::path p(key);
    // build any missing parts
    auto iter = p.begin();
    for (iter = p.begin(); iter != p.end(); ++iter) {
        if (true) {}
    }
}


QStringList DsSettings::getSettingsNames() {
    QStringList names;
    for (const auto& [name, ref] : sSettings) {
        names.append(QString::fromStdString(name));
    }
    names.sort(Qt::CaseInsensitive);
    return names;
}

QStringList DsSettings::getLoadedFiles() const {
    QStringList files;
    for (const auto& sf : mResultStack) {
        files.append(QString::fromStdString(sf.filepath));
    }
    if (mRuntimeResult.valid) {
        files.append(QStringLiteral("runtime"));
    }
    return files;
}

QString DsSettings::tomlNodeToDisplayString(const toml::node& node) {
    if (node.is_string())         return QString::fromStdString(*node.value<std::string>());
    if (node.is_integer())        return QString::number(*node.value<int64_t>());
    if (node.is_floating_point()) return QString::number(*node.value<double>());
    if (node.is_boolean())        return *node.value<bool>() ? QStringLiteral("true") : QStringLiteral("false");
    if (node.is_date())           { std::ostringstream ss; ss << *node.value<toml::date>(); return QString::fromStdString(ss.str()); }
    if (node.is_time())           { std::ostringstream ss; ss << *node.value<toml::time>(); return QString::fromStdString(ss.str()); }
    if (node.is_date_time())      { std::ostringstream ss; ss << *node.value<toml::date_time>(); return QString::fromStdString(ss.str()); }
    if (node.is_array()) {
        std::ostringstream ss;
        ss << *node.as_array();
        return QString::fromStdString(ss.str());
    }
    return QStringLiteral("<unknown>");
}

QString DsSettings::tomlNodeTypeName(const toml::node& node) {
    switch (node.type()) {
        case toml::node_type::string:         return QStringLiteral("string");
        case toml::node_type::integer:        return QStringLiteral("integer");
        case toml::node_type::floating_point: return QStringLiteral("float");
        case toml::node_type::boolean:        return QStringLiteral("bool");
        case toml::node_type::date:           return QStringLiteral("date");
        case toml::node_type::time:           return QStringLiteral("time");
        case toml::node_type::date_time:      return QStringLiteral("datetime");
        case toml::node_type::array:          return QStringLiteral("array");
        case toml::node_type::table:          return QStringLiteral("table");
        default:                              return QStringLiteral("unknown");
    }
}

void DsSettings::buildSettingsMap(const toml::table& table, const QString& source, QVariantMap& output) const {
    for (const auto& [key, nodeRef] : table) {
        QString keyStr = QString::fromStdString(std::string(key.str()));

        if (nodeRef.is_table()) {
            // Recurse into sub-table
            QVariantMap subMap;
            if (output.contains(keyStr) && output[keyStr].typeId() == QMetaType::QVariantMap) {
                subMap = output[keyStr].toMap();
            }
            buildSettingsMap(*nodeRef.as_table(), source, subMap);
            output[keyStr] = subMap;
        } else if (nodeRef.is_array() && nodeRef.as_array()->size() > 1
                   && nodeRef.as_array()->back().is_table()) {
            // Metadata convention: [value, {meta}] — use element[0] as value
            const auto& valueNode = nodeRef.as_array()->at(0);
            QVariantMap leaf;
            leaf[QStringLiteral("value")]    = tomlNodeToDisplayString(valueNode);
            leaf[QStringLiteral("source")]   = source;
            leaf[QStringLiteral("type")]     = tomlNodeTypeName(valueNode);
            leaf[QStringLiteral("__isLeaf")] = true;
            output[keyStr] = leaf;
        } else {
            // Regular leaf value
            QVariantMap leaf;
            leaf[QStringLiteral("value")]    = tomlNodeToDisplayString(nodeRef);
            leaf[QStringLiteral("source")]   = source;
            leaf[QStringLiteral("type")]     = tomlNodeTypeName(nodeRef);
            leaf[QStringLiteral("__isLeaf")] = true;
            output[keyStr] = leaf;
        }
    }
}

QVariantMap DsSettings::getSettingsTree(const QString& filterFile) const {
    QVariantMap result;

    if (filterFile.isEmpty()) {
        // Merge all files bottom-to-top (later files override)
        for (const auto& sf : mResultStack) {
            if (!sf.valid) continue;
            if (const auto* tbl = sf.data.as_table()) {
                buildSettingsMap(*tbl, QString::fromStdString(sf.filepath), result);
            }
        }
        // Runtime overrides last
        if (mRuntimeResult.valid) {
            if (const auto* tbl = mRuntimeResult.data.as_table()) {
                buildSettingsMap(*tbl, QStringLiteral("runtime"), result);
            }
        }
    } else {
        // Find the specific file
        std::string filter = filterFile.toStdString();
        for (const auto& sf : mResultStack) {
            if (sf.filepath == filter && sf.valid) {
                if (const auto* tbl = sf.data.as_table()) {
                    buildSettingsMap(*tbl, filterFile, result);
                }
                return result;
            }
        }
        // Check runtime
        if (filterFile == QStringLiteral("runtime") && mRuntimeResult.valid) {
            if (const auto* tbl = mRuntimeResult.data.as_table()) {
                buildSettingsMap(*tbl, QStringLiteral("runtime"), result);
            }
        }
    }

    return result;
}

//-----------------------------------------------------------------------

// Convert a toml::node_view<const toml::node> to QVariant. this function
QVariant DsSettings::tomlNodeViewToQVariant(const toml::node_view<const toml::node>& n) {
    // Boolean?
    if (n.is_boolean()) {
        // n.value<bool>() returns std::optional<bool>
        std::optional<bool> optBool = n.value<bool>();
        if (optBool.has_value()) return QVariant(optBool.value());
        return QVariant();
    }
    // Integer?
    else if (n.is_integer()) {
        std::optional<toml::int64_t> optInt = n.value<toml::int64_t>();
        if (optInt.has_value()) return QVariant(static_cast<qint64>(optInt.value()));
        return QVariant();
    }
    // Floating-point?
    else if (n.is_floating_point()) {
        std::optional<double> optDouble = n.value<double>();
        if (optDouble.has_value()) return QVariant(optDouble.value());
        return QVariant();
    }
    // String?
    else if (n.is_string()) {
        std::optional<std::string> optStr = n.value<std::string>();
        if (optStr.has_value()) return QVariant(QString::fromStdString(optStr.value()));
        return QVariant();
    }
    // Date-time?
    else if (n.is_date_time()) {
        std::optional<toml::date_time> optDT = n.value<toml::date_time>();
        if (optDT.has_value()) {
            toml::date_time dt = optDT.value();
            QDate           qd(dt.date.year, dt.date.month, dt.date.day);
            QTime           qt(dt.time.hour, dt.time.minute, dt.time.second);
            QDateTime       qdt(qd, qt,
                          dt.offset.has_value() ? QTimeZone(dt.offset->minutes * 60)
                                                      : QTimeZone(QTimeZone::Initialization::LocalTime));
            return QVariant::fromValue(qdt);
        }
        return QVariant();
    }
    // Array?
    else if (n.is_array()) {
        if (auto arrPtr = n.as_array()) {
            // Construct a node_view over the array, then recurse
            return QVariant::fromValue(tomlArrayViewToVariantList(toml::node_view<const toml::node>(*arrPtr)));
        }
        return QVariant();
    }
    // Table?
    else if (n.is_table()) {
        if (auto tabPtr = n.as_table()) {
            // Construct a node_view over the table, then recurse
            return QVariant::fromValue(tomlTableViewToVariantMap(toml::node_view<const toml::node>(*tabPtr)));
        }
        return QVariant();
    }

    // Fallback: unrecognized type → invalid QVariant
    return QVariant();
}

// Recursively convert a toml::node_view<const toml::array> into QVariantList
QVariantList DsSettings::tomlArrayViewToVariantList(const toml::node_view<const toml::node>& arrView) {
    QVariantList list;
    // list.reserve(static_cast<int>(arrView->size()));

    for (auto const& element : *arrView.as_array()) {
        // For each element (a toml::node), create a node_view and recurse
        list.append(tomlNodeViewToQVariant(toml::node_view<const toml::node>(element)));
    }
    return list;
}

// Recursively convert a toml::node_view<const toml::table> into QVariantMap
QVariantMap DsSettings::tomlTableViewToVariantMap(const toml::node_view<const toml::node>& tabView) {
    QVariantMap map;
    // map.reserve(static_cast<int>(tabView->size()));

    for (auto const& [key, nodeRef] : *tabView.as_table()) {
        // For each key/value, wrap the node in a node_view and recurse
        map.insert(QString::fromStdString(std::string(key.str())),
                   tomlNodeViewToQVariant(toml::node_view<const toml::node>(nodeRef)));
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


} // namespace dsqt

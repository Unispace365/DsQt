#ifndef DSSETTINGS_H
#define DSSETTINGS_H


#include "QtGui/qcolor.h"
#include <QQmlPropertyMap>
#include <qqml.h>
#include <toml++/toml.h>

/**
 * @brief The DSSettings class
 * Reads a setting file
 */
namespace dsqt {

class DSSettings;
using DSSettingsRef = std::shared_ptr<DSSettings>;
using parse_resultWRef = std::weak_ptr<toml::parse_result>;
using NodeWMeta = std::tuple<toml::node_view<toml::node>,toml::table*,std::string>;
template<typename T> using ValueWMeta = std::tuple<T,toml::table*,std::string>;

class DSSettings:public QObject {
    Q_OBJECT
public:
    //struct to hold setting file
    struct SettingFile {
        std::string filepath;
        toml::parse_result data;
        bool valid = false;
    };
protected:
    DSSettings(std::string name="",QObject* parent=nullptr);
public:
    DSSettings(QObject* parent=nullptr)=delete;
    ~DSSettings();

    /// load a setting file into this setting object.
    /// \return a bool the indecates the success
    /// or failure of loading the file.
    bool loadSettingFile(const std::string &file);

    /// Get a setting collection by name.
    /// Get the setting file associated with \b name.
    /// If the setting file does not exist it will create it.
    /// \returns a tuple of [bool \b exists,DSSettingRef \b settings]
    static std::tuple<bool,DSSettingsRef> getSettingsOrCreate(const std::string& name, QObject* parent=nullptr);
    static DSSettingsRef getSettings(const std::string& name);

    ///Get a setting from the collection.
    template<class T> T getOr(const std::string& key,const T& def);
    template<class T> std::optional<T> getValue(const std::string& key){
        auto retval = getWithMeta<T>(key);
        if(retval){
           auto [value,x,y] = retval.value();
           return value;
        }
        return std::optional<T>();
    }
    //template<> std::optional<std::string> getValue(const std::string& key);

    ///Get a setting from the collection with the meta data.
    ///This is used for debuging and internal management.
    std::optional<NodeWMeta> getNodeViewWithMeta(const std::string& key);
    template<class T> std::optional<ValueWMeta<T>> getWithMeta(const std::string& key){
        auto val = getNodeViewWithMeta(key);
        if(!val.has_value()) return std::optional<ValueWMeta<T>>();

        auto [node,meta,place] = val.value();
        if(node.is<T>()){
            return std::optional<ValueWMeta<T>>(ValueWMeta<T>(node.value_or(T()),meta,place));
        }
        return std::optional<ValueWMeta<T>>();
    }
    template<> std::optional<ValueWMeta<QColor>> getWithMeta(const std::string& key);
    //Set a value in the collection.
    template<class T> void set(std::string& key,T &value);


    // QQmlPropertyMap interface
private:
    //meta data
    static std::string mConfigurationDirectory;
    static bool mLoadedConfiguration;
    static std::unordered_map<std::string,DSSettingsRef> sSettings;
    std::string mName="";
    std::vector<SettingFile> mResultStack;
    SettingFile mRuntimeResult;
    static std::unordered_map<std::string,std::function<void(QColor& resultcolor,float v1,float v2,float v3,float v4,float v5)>> sColorConversionFuncs;
};

}//namespace dsqt
#endif // DSSETTINGS_H

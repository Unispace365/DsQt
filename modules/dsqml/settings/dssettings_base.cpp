#include <dssettings.h>
#include <string>
#include <algorithm>

Q_LOGGING_CATEGORY(settingsParser, "settings.parser")
namespace dsqt {


template<> std::optional<ValueWMeta<std::string>> DSSettings::getWithMeta(const std::string& key){

    auto val = getNodeViewWithMeta(key);
    if(!val.has_value()){
        qDebug(settingsParser)<<"Failed to find value at key "<<key.c_str();
        return std::nullopt;
    }

    auto [n,m,p] = val.value();
    auto node =n;
    auto meta= m;
    auto place = p;
    auto grabber = DSOverloaded {
    [meta,place](toml::value<std::string> s){ return std::optional(ValueWMeta<std::string>(s.get(),meta,place));},
    [meta,place](toml::value<bool> s){ return std::optional(ValueWMeta<std::string>(s.get()?"true":"false",meta,place));},
    [meta,place](toml::value<int64_t> s){ return std::optional(ValueWMeta<std::string>(std::to_string(s.get()),meta,place));},
    [meta,place](toml::value<double> s){ return std::optional(ValueWMeta<std::string>(std::to_string(s.get()),meta,place));},
    [meta,place](toml::value<toml::date> s){
        toml::date date = s.get();
        std::stringstream ss;
        ss << date;
        return std::optional(ValueWMeta<std::string>(ss.str(),meta,place));},
    [meta,place](toml::value<toml::time> s){
        toml::time time = s.get();
        std::stringstream ss;
        ss << time;
        return std::optional(ValueWMeta<std::string>(ss.str(),meta,place));},
    [meta,place](toml::value<toml::date_time> s){
        toml::date_time dateTime = s.get();
        std::stringstream ss;
        ss << dateTime;
        return std::optional(ValueWMeta<std::string>(ss.str(),meta,place));},
    [meta,place](toml::array s){ return std::optional<ValueWMeta<std::string>>();},
    [meta,place](toml::table s){ return std::optional<ValueWMeta<std::string>>();}
};
    auto nodeval = node.visit(grabber);
    return nodeval;

}

template<> std::optional<ValueWMeta<QString>> DSSettings::getWithMeta(const std::string& key){
    auto temp = getWithMeta<std::string>(key);
    if(temp.has_value()){
        auto [val,meta,place] = temp.value();
        return std::optional(ValueWMeta<QString>(QString::fromStdString(val),meta,place));
    }
    return std::nullopt;
}

template<> std::optional<ValueWMeta<int64_t>> DSSettings::getWithMeta(const std::string& key){
    qCDebug(settingsParser)<<"RUNNING int64";
    auto val = getNodeViewWithMeta(key);
    if(!val.has_value()) return std::optional<ValueWMeta<int64_t>>();

    auto [node,meta,place] = val.value();
    if(node.is_integer()){
        return std::optional<ValueWMeta<int64_t>>(ValueWMeta<int64_t>(node.value_or<int64_t>(0),meta,place));
    } else if(node.is_floating_point()){
        return std::optional<ValueWMeta<int64_t>>(ValueWMeta<int64_t>(node.value_or<double>(0),meta,place));
    } else if(node.is_string()) {
        return std::optional<ValueWMeta<int64_t>>(ValueWMeta<int64_t>(std::stoll(node.value_or<std::string>("")),meta,place));
    }
    return std::optional<ValueWMeta<int64_t>>();
}

template<> std::optional<ValueWMeta<uint64_t>> DSSettings::getWithMeta(const std::string& key){
    qCDebug(settingsParser)<<"RUNNING uint64";
    auto val = getNodeViewWithMeta(key);
    if(!val.has_value()) return std::optional<ValueWMeta<uint64_t>>();

    auto [node,meta,place] = val.value();
    if(node.is_integer()){
        return std::optional<ValueWMeta<uint64_t>>(ValueWMeta<uint64_t>(node.value_or<uint64_t>(0),meta,place));
    } else if(node.is_floating_point()){
        return std::optional<ValueWMeta<uint64_t>>(ValueWMeta<uint64_t>(node.value_or<double>(0),meta,place));
    } else if(node.is_string()) {
        return std::optional<ValueWMeta<uint64_t>>(ValueWMeta<uint64_t>(std::stoull(node.value_or<std::string>("")),meta,place));
    }
    return std::optional<ValueWMeta<uint64_t>>();
}

template<> std::optional<ValueWMeta<int32_t>> DSSettings::getWithMeta(const std::string& key){
    qCDebug(settingsParser)<<"RUNNING int32";
    auto val = getNodeViewWithMeta(key);
    if(!val.has_value()) return std::optional<ValueWMeta<int32_t>>();

    auto [node,meta,place] = val.value();
    if(node.is_integer()){
        return std::optional<ValueWMeta<int32_t>>(ValueWMeta<int32_t>(node.value_or<int64_t>(0),meta,place));
    } else if(node.is_floating_point()){
        return std::optional<ValueWMeta<int32_t>>(ValueWMeta<int32_t>(node.value_or<double>(0),meta,place));
    } else if(node.is_string()) {
        return std::optional<ValueWMeta<int32_t>>(ValueWMeta<int32_t>(std::stoi(node.value_or<std::string>("")),meta,place));
    }
    return std::optional<ValueWMeta<int32_t>>();
}
template<> std::optional<ValueWMeta<uint32_t>> DSSettings::getWithMeta(const std::string& key){
    qCDebug(settingsParser)<<"RUNNING uint32";
    auto val = getNodeViewWithMeta(key);
    if(!val.has_value()) return std::optional<ValueWMeta<uint32_t>>();

    auto [node,meta,place] = val.value();
    if(node.is_integer()){
        return std::optional<ValueWMeta<uint32_t>>(ValueWMeta<uint32_t>(node.value_or<uint64_t>(0),meta,place));
    } else if(node.is_floating_point()){
        return std::optional<ValueWMeta<uint32_t>>(ValueWMeta<uint32_t>(node.value_or<double>(0),meta,place));
    } else if(node.is_string()) {
        return std::optional<ValueWMeta<uint32_t>>(ValueWMeta<uint32_t>(std::stoul(node.value_or<std::string>("")),meta,place));
    }
    return std::optional<ValueWMeta<uint32_t>>();
}


template<> std::optional<ValueWMeta<double>> DSSettings::getWithMeta(const std::string& key){
    qCDebug(settingsParser)<<"RUNNING double";
    auto val = getNodeViewWithMeta(key);
    if(!val.has_value()) return std::optional<ValueWMeta<double>>();

    auto [node,meta,place] = val.value();
    if(node.is_floating_point()){
        return std::optional<ValueWMeta<double>>(ValueWMeta<double>(node.value_or<double>(0),meta,place));
    } else if(node.is_integer()){
        return std::optional<ValueWMeta<double>>(ValueWMeta<double>(node.value_or<int64_t>(0),meta,place));
    } else if(node.is_string()) {
        return std::optional<ValueWMeta<double>>(ValueWMeta<double>(std::stod(node.value_or<std::string>("")),meta,place));
    }
    return std::optional<ValueWMeta<double>>();
}

template<> std::optional<ValueWMeta<float>> DSSettings::getWithMeta(const std::string& key){
    qCDebug(settingsParser)<<"RUNNING float";
    auto val = getNodeViewWithMeta(key);
    if(!val.has_value()) return std::optional<ValueWMeta<float>>();

    auto [node,meta,place] = val.value();
    if(node.is_floating_point()){
        return std::optional<ValueWMeta<float>>(ValueWMeta<float>(node.value_or<float>(0),meta,place));
    } else if(node.is_integer()){
        return std::optional<ValueWMeta<float>>(ValueWMeta<float>(node.value_or<int64_t>(0),meta,place));
    } else if(node.is_string()) {
        return std::optional<ValueWMeta<float>>(ValueWMeta<float>(std::stof(node.value_or<std::string>("")),meta,place));
    }
    return std::optional<ValueWMeta<float>>();
}

template<> std::optional<ValueWMeta<bool>> DSSettings::getWithMeta(const std::string& key){
    qCDebug(settingsParser)<<"RUNNING float";
    auto val = getNodeViewWithMeta(key);
    if(!val.has_value()) return std::optional<ValueWMeta<float>>();

    auto [node,meta,place] = val.value();
    if(node.is_boolean()){
        return std::optional<ValueWMeta<bool>>(ValueWMeta<bool>(node.value_or<bool>(false),meta,place));
    }else if(node.is_floating_point()){
        return std::optional<ValueWMeta<bool>>(ValueWMeta<bool>((bool)node.value_or<double>(0),meta,place));
    } else if(node.is_integer()){
        return std::optional<ValueWMeta<bool>>(ValueWMeta<bool>((bool)node.value_or<int64_t>(0),meta,place));
    } else if(node.is_string()) {
        std::string bool_str = node.value_or<std::string>("");
        std::transform(bool_str.begin(),bool_str.end(),bool_str.begin(),std::tolower);

        bool ret_val = bool_str == "false" || bool_str=="" || bool_str=="0"?false:true;
        return std::optional<ValueWMeta<bool>>(ValueWMeta<bool>(ret_val,meta,place));
    }
    return std::optional<ValueWMeta<float>>();
}

}

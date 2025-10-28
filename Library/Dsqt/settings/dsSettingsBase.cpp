#include "settings/dsSettings.h"

#include <string>
#include <algorithm>

namespace dsqt {


template<> MaybeStringMeta DsSettings::getWithMeta(const std::string& key){

    auto val = getNodeViewWithMeta(key);
    if(!val.has_value()){
        qDebug(lgSPVerbose)<<"Failed to find value at key "<<key.c_str();
        return std::nullopt;
    }

    auto [n,m,p] = val.value();
    auto node =n;
    auto meta= m;
    auto place = p;
    auto grabber = DSOverloaded {
    [meta, place](toml::value<std::string> s){ return std::optional(ValueWMeta<std::string>(s.get(), meta, place));},
    [meta, place](toml::value<bool> s){ return std::optional(ValueWMeta<std::string>(s.get()?"true":"false", meta, place));},
    [meta, place](toml::value<int64_t> s){ return std::optional(ValueWMeta<std::string>(std::to_string(s.get()), meta, place));},
    [meta, place](toml::value<double> s){ return std::optional(ValueWMeta<std::string>(std::to_string(s.get()), meta, place));},
    [meta, place](toml::value<toml::date> s){
        toml::date date = s.get();
        std::stringstream ss;
        ss << date;
        return std::optional(ValueWMeta<std::string>(ss.str(), meta, place));},
    [meta, place](toml::value<toml::time> s){
        toml::time time = s.get();
        std::stringstream ss;
        ss << time;
        return std::optional(ValueWMeta<std::string>(ss.str(), meta, place));},
    [meta, place](toml::value<toml::date_time> s){
        toml::date_time dateTime = s.get();
        std::stringstream ss;
        ss << dateTime;
        return std::optional(ValueWMeta<std::string>(ss.str(), meta, place));},
    [meta, place](toml::array s){
        toml::array array = s;
        std::stringstream ss;
        ss << array;
        return std::optional(ValueWMeta<std::string>(ss.str(), meta, place));},
    [meta, place](toml::table s){
            toml::table table = s;
            std::stringstream ss;
            ss << table;
            return std::optional(ValueWMeta<std::string>(ss.str(), meta, place));}
    };
    auto nodeval = node.visit(grabber);
    return nodeval;

}

template<> MaybeQStringMeta DsSettings::getWithMeta(const std::string& key){
    auto temp = getWithMeta<std::string>(key);
    if(temp.has_value()){
        auto [val, meta, place] = temp.value();
        return std::optional(ValueWMeta<QString>(QString::fromStdString(val), meta, place));
    }
    return std::nullopt;
}

template<> MaybeInt64Meta DsSettings::getWithMeta(const std::string& key){
    qCDebug(lgSPVerbose) << "RUNNING int64";

    auto val = getNodeViewWithMeta(key);
    if(!val.has_value()) return MaybeInt64Meta();

    auto [node, meta, place] = val.value();
    if(node.is_integer()){
        return MaybeInt64Meta( {node.ref<int64_t>(), meta, place} );
    } else if(node.is_floating_point()){
        return MaybeInt64Meta( {static_cast<int64_t>(node.ref<double>()), meta, place} );
    } else if(node.is_string()) {
        int64_t int64_val;
        try {
            int64_val = std::stoll(node.value_or<std::string>(""));
        } catch (std::invalid_argument exception) {
            // NAN cast to int64_t is zero
            int64_val = 0;
        } catch (std::out_of_range exception) {
            int64_val = std::numeric_limits<int64_t>::max();
        }
        return MaybeInt64Meta( {int64_val, meta, place} );
    }
    return MaybeInt64Meta();
}

template<> MaybeInt32Meta DsSettings::getWithMeta(const std::string& key){
    qCDebug(lgSPVerbose) << "RUNNING int32";

    auto val = getNodeViewWithMeta(key);
    if(!val.has_value()) return MaybeInt32Meta();

    auto [node, meta, place] = val.value();
    if(node.is_integer()){
        return MaybeInt32Meta( {static_cast<int32_t>(node.ref<int64_t>()), meta, place} );
    } else if(node.is_floating_point()){
        return MaybeInt32Meta( {static_cast<int32_t>(node.ref<double>()), meta, place} );
    } else if(node.is_string()) {
        int32_t int32_val;
        try {
            int32_val = std::stoi(node.value_or<std::string>(""));
        } catch (std::invalid_argument exception) {
            // NAN cast to int32_t is zero
            int32_val = 0;
        } catch (std::out_of_range exception) {
            int32_val = std::numeric_limits<int32_t>::max();
        }
        return MaybeInt32Meta( {int32_val, meta, place} );
    }
    return MaybeInt32Meta();
}

template<> MaybeDoubleMeta DsSettings::getWithMeta(const std::string& key){
    qCDebug(lgSPVerbose) << "RUNNING double";
    auto val = getNodeViewWithMeta(key);
    if(!val.has_value()) return MaybeDoubleMeta();

    auto [node, meta, place] = val.value();
    if(node.is_floating_point()){
        return MaybeDoubleMeta( { node.ref<double>(), meta, place } );
    } else if(node.is_integer()){
        return MaybeDoubleMeta( { static_cast<double>(node.ref<int64_t>()), meta, place } );
    } else if(node.is_string()) {
        double double_val;
        try {
            double_val = std::stod(node.value_or<std::string>(""));
        } catch (std::invalid_argument exception) {
            double_val = std::numeric_limits<double>::quiet_NaN();
        } catch (std::out_of_range exception) {
            double_val = std::numeric_limits<double>::infinity();
        }

        return MaybeDoubleMeta({double_val, meta, place});
    }
    return MaybeDoubleMeta();
}

template<> MaybeFloatMeta DsSettings::getWithMeta(const std::string& key){
    qCDebug(lgSPVerbose) << "RUNNING float";
    auto val = getNodeViewWithMeta(key);
    if(!val.has_value()) return MaybeFloatMeta();

    auto [node, meta, place] = val.value();
    if(node.is_floating_point()){
        return MaybeFloatMeta( {static_cast<float>(node.ref<double>()), meta, place} );
    } else if(node.is_integer()){
        return MaybeFloatMeta( {static_cast<float>(node.ref<double>()), meta, place} );
    } else if(node.is_string()) {
        double double_val;
        try {
            double_val = std::stod(node.value_or<std::string>(""));
        } catch (std::invalid_argument e) {
            double_val = std::numeric_limits<float>::quiet_NaN();
        }catch (std::out_of_range exception) {
            double_val = std::numeric_limits<float>::infinity();
        }

        return MaybeFloatMeta( {static_cast<float>(double_val), meta, place} );
    }
    return MaybeFloatMeta();
}

template<> MaybeBoolMeta DsSettings::getWithMeta(const std::string& key){
    qCDebug(lgSPVerbose) << "RUNNING bool";
    auto val = getNodeViewWithMeta(key);
    if(!val.has_value()) return std::optional<ValueWMeta<float>>();

    auto [node, meta, place] = val.value();
    if(node.is_boolean()){
        return MaybeBoolMeta( {node.ref<bool>(), meta, place} );
    }else if(node.is_floating_point()){
        return MaybeBoolMeta( {static_cast<double>(node.ref<bool>()), meta, place} );
    } else if(node.is_integer()){
        return MaybeBoolMeta( {static_cast<double>(node.ref<int64_t>()), meta, place} );
    } else if(node.is_string()) {
        std::string bool_str = node.value_or<std::string>("");
        std::transform(bool_str.begin(),bool_str.end(),bool_str.begin(),[](unsigned char c){ return std::tolower(c);});

        bool ret_val = bool_str == "false" || bool_str=="" || bool_str =="0"?false:true;
        return MaybeBoolMeta( {ret_val, meta, place} );
    }
    return MaybeBoolMeta();
}


}

#include "settings/dsSettings.h"

#include <string>
#include <algorithm>

namespace dsqt {


template<> std::optional<ValueWMeta<std::string>> DsSettings::getWithMeta(const std::string& key){

    auto val = getNodeViewWithMeta(key);
    if(!val.has_value()){
		qDebug(lgSettingsParser)<<"Failed to find value at key "<<key.c_str();
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
	[meta,place](toml::array s){
		toml::array array = s;
		std::stringstream ss;
		ss << array;
		return std::optional(ValueWMeta<std::string>(ss.str(),meta,place));},
	[meta,place](toml::table s){
			toml::table table = s;
			std::stringstream ss;
			ss << table;
			return std::optional(ValueWMeta<std::string>(ss.str(),meta,place));}
};
    auto nodeval = node.visit(grabber);
    return nodeval;

}

template<> std::optional<ValueWMeta<QString>> DsSettings::getWithMeta(const std::string& key){
    auto temp = getWithMeta<std::string>(key);
    if(temp.has_value()){
        auto [val,meta,place] = temp.value();
        return std::optional(ValueWMeta<QString>(QString::fromStdString(val),meta,place));
    }
    return std::nullopt;
}

template<> std::optional<ValueWMeta<int64_t>> DsSettings::getWithMeta(const std::string& key){
	qCDebug(lgSPVerbose) << "RUNNING int64";
	auto val = getNodeViewWithMeta(key);
    if(!val.has_value()) return std::optional<ValueWMeta<int64_t>>();

    auto [node,meta,place] = val.value();
    if(node.is_integer()){
		return std::optional<ValueWMeta<int64_t>>(ValueWMeta<int64_t>(node.value_or<int64_t>(INFINITY),meta,place));
    } else if(node.is_floating_point()){
		return std::optional<ValueWMeta<int64_t>>(ValueWMeta<int64_t>(node.value_or<double>(INFINITY),meta,place));
    } else if(node.is_string()) {
		int64_t int64_val;
		try {
			int64_val = std::stoll(node.value_or<std::string>(""));
		} catch (std::invalid_argument exception) {
			int64_val = NAN;
		} catch (std::out_of_range exception) {
			int64_val = INFINITY;
		}
		return std::optional<ValueWMeta<int64_t>>(ValueWMeta<int64_t>(int64_val,meta,place));
    }
    return std::optional<ValueWMeta<int64_t>>();
}

template<> std::optional<ValueWMeta<int32_t>> DsSettings::getWithMeta(const std::string& key){
	qCDebug(lgSPVerbose) << "RUNNING int32";
	auto val = getNodeViewWithMeta(key);
    if(!val.has_value()) return std::optional<ValueWMeta<int32_t>>();

    auto [node,meta,place] = val.value();
    if(node.is_integer()){
		return std::optional<ValueWMeta<int32_t>>(ValueWMeta<int32_t>(node.value_or<int64_t>(INFINITY),meta,place));
    } else if(node.is_floating_point()){
		return std::optional<ValueWMeta<int32_t>>(ValueWMeta<int32_t>(node.value_or<double>(INFINITY),meta,place));
    } else if(node.is_string()) {
		int32_t int32_val;
		try {
			int32_val = std::stoi(node.value_or<std::string>(""));
		} catch (std::invalid_argument exception) {
			int32_val = NAN;
		} catch (std::out_of_range exception) {
			int32_val = INFINITY;
		}
		return std::optional<ValueWMeta<int32_t>>(ValueWMeta<int32_t>(int32_val,meta,place));
    }
    return std::optional<ValueWMeta<int32_t>>();
}

template<> std::optional<ValueWMeta<double>> DsSettings::getWithMeta(const std::string& key){
	qCDebug(lgSPVerbose) << "RUNNING double";
	auto val = getNodeViewWithMeta(key);
    if(!val.has_value()) return std::optional<ValueWMeta<double>>();

    auto [node,meta,place] = val.value();
    if(node.is_floating_point()){
		return std::optional<ValueWMeta<double>>(ValueWMeta<double>(node.value_or<double>(INFINITY),meta,place));
    } else if(node.is_integer()){
		return std::optional<ValueWMeta<double>>(ValueWMeta<double>(node.value_or<int64_t>(INFINITY),meta,place));
    } else if(node.is_string()) {
		double double_val;
		try {
			double_val = std::stod(node.value_or<std::string>(""));
		} catch (std::invalid_argument exception) {
			double_val = NAN;
		} catch (std::out_of_range exception) {
			double_val = INFINITY;
		}

		return std::optional<ValueWMeta<double>>(ValueWMeta<double>(double_val,meta,place));
    }
    return std::optional<ValueWMeta<double>>();
}

template<> std::optional<ValueWMeta<float>> DsSettings::getWithMeta(const std::string& key){
	qCDebug(lgSPVerbose) << "RUNNING float";
	auto val = getNodeViewWithMeta(key);
    if(!val.has_value()) return std::optional<ValueWMeta<float>>();

    auto [node,meta,place] = val.value();
    if(node.is_floating_point()){
		auto double_val = node.value_or<double>(NAN);
		return std::optional<ValueWMeta<float>>(ValueWMeta<float>((float)double_val,meta,place));
    } else if(node.is_integer()){
		return std::optional<ValueWMeta<float>>(ValueWMeta<float>(node.value_or<int64_t>(INFINITY),meta,place));
    } else if(node.is_string()) {
		double double_val;
		try {
			double_val = std::stod(node.value_or<std::string>(""));
		} catch (std::invalid_argument e) {
			double_val = NAN;
		}catch (std::out_of_range exception) {
			double_val = INFINITY;
		}

		return std::optional<ValueWMeta<float>>(ValueWMeta<float>((float)double_val,meta,place));
    }
    return std::optional<ValueWMeta<float>>();
}

template<> std::optional<ValueWMeta<bool>> DsSettings::getWithMeta(const std::string& key){
    qCDebug(lgSPVerbose) << "RUNNING bool";
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
        std::transform(bool_str.begin(),bool_str.end(),bool_str.begin(),[](unsigned char c){ return std::tolower(c);});

        bool ret_val = bool_str == "false" || bool_str=="" || bool_str=="0"?false:true;
        return std::optional<ValueWMeta<bool>>(ValueWMeta<bool>(ret_val,meta,place));
    }
    return std::optional<ValueWMeta<float>>();
}



}

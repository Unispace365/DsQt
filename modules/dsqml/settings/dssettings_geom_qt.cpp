#include <dssettings.h>
#include <string>
#include <algorithm>

namespace dsqt {
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

#include "settings.h"
#include <string>
#include <algorithm>


namespace dsqt {

using toml_nv = toml::node_view<toml::node>;
template<> std::optional<ValueWMeta<toml_nv>> DSSettings::getWithMeta(const std::string& key){

    auto val = getNodeViewWithMeta(key);
    if(!val.has_value()){
        qDebug(settingsParser)<<"Failed to find value at key "<<key.c_str();
        return std::optional<ValueWMeta<toml_nv>>();
    }

    auto [n,m,p] = val.value();
    auto node =n;
    auto meta= m;
    auto place = p;

    return std::optional<ValueWMeta<toml_nv>>(ValueWMeta<toml_nv>(node,meta,place));;

}

}

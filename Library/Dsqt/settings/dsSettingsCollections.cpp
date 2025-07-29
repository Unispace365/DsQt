#include "dsSettings.h"
#include <string>
#include <algorithm>


namespace dsqt {

using toml_nv = toml::node_view<toml::node>;
template<> std::optional<ValueWMeta<toml_nv>> DsSettings::getWithMeta(const std::string& key){

    auto val = getNodeViewWithMeta(key);
    if(!val.has_value()){
		qDebug(lgSettingsParser)<<"Failed to find value at key "<<key.c_str();
        return std::optional<ValueWMeta<toml_nv>>();
    }

    auto [n,m,p] = val.value();
    auto node =n;
    auto meta= m;
    auto place = p;

    return std::optional<ValueWMeta<toml_nv>>(ValueWMeta<toml_nv>(node,meta,place));;

}

template<> std::optional<ValueWMeta<QVariantList>> DsSettings::getWithMeta(const std::string& key){
    auto val = getNodeViewWithMeta(key);
    if(!val.has_value()){
        qDebug(lgSettingsParser)<<"Failed to find value at key "<<key.c_str();
        return std::optional<ValueWMeta<QVariantList>>();
    }

    auto [n,m,p] = val.value();

    const auto q = n.node();

    const toml::node_view<const toml::node> node =toml::node_view<const toml::node>(*q);
    auto meta= m;
    auto place = p;

    auto outVal = tomlNodeViewToQVariant(node);
    if(outVal.canConvert<QVariantList>()){
        return std::optional<ValueWMeta<QVariantList>>(ValueWMeta<QVariantList>(outVal.toList(),meta,place));
    }
    return std::optional<ValueWMeta<QVariantList>>(ValueWMeta<QVariantList>(QVariantList(),meta,place));
}

template<> std::optional<ValueWMeta<QVariantMap>> DsSettings::getWithMeta(const std::string& key){
    auto val = getNodeViewWithMeta(key);
    if(!val.has_value()){
        qDebug(lgSettingsParser)<<"Failed to find value at key "<<key.c_str();
        return std::optional<ValueWMeta<QVariantMap>>();
    }

    auto [n,m,p] = val.value();

    const auto q = n.node();

    const toml::node_view<const toml::node> node =toml::node_view<const toml::node>(*q);
    auto meta= m;
    auto place = p;

    auto outVal = tomlNodeViewToQVariant(node);
    if(outVal.canConvert<QVariantMap>()){
        return std::optional<ValueWMeta<QVariantMap>>(ValueWMeta<QVariantMap>(outVal.toMap(),meta,place));
    }
    return std::optional<ValueWMeta<QVariantMap>>(ValueWMeta<QVariantMap>(QVariantMap(),meta,place));
}

}

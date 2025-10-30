#include "settings/dsSettings.h"

#include <string>

namespace dsqt {

using toml_nv = toml::node_view<toml::node>;
template<> std::optional<ValueWMeta<toml_nv>> DsSettings::getWithMeta(const std::string& key){

    auto val = getNodeViewWithMeta(key);
    if(!val.has_value()){
        qDebug(lgSPVerbose)<<"Failed to find value at key "<<key.c_str();
        return std::optional<ValueWMeta<toml_nv>>();
    }

    auto [n,m,p] = val.value();
    auto node =n;
    auto meta= m;
    auto place = p;

    return std::optional<ValueWMeta<toml_nv>>(ValueWMeta<toml_nv>(node, meta, place));;

}

template<> MaybeQVariantListMeta DsSettings::getWithMeta(const std::string& key){
    auto val = getNodeViewWithMeta(key);
    if(!val.has_value()){
        qDebug(lgSPVerbose)<<"Failed to find value at key "<<key.c_str();
        return MaybeQVariantListMeta();
    }

    auto [n,m,p] = val.value();

    const auto q = n.node();

    const toml::node_view<const toml::node> node =toml::node_view<const toml::node>(*q);
    auto meta= m;
    auto place = p;

    auto outVal = tomlNodeViewToQVariant(node);
    if(outVal.canConvert<QVariantList>()){
        return MaybeQVariantListMeta( {outVal.toList(), meta, place} );
    }
    return MaybeQVariantListMeta( {QVariantList(), meta, place} );
}

template<> MaybeQVariantMapMeta DsSettings::getWithMeta(const std::string& key){
    auto val = getNodeViewWithMeta(key);
    if(!val.has_value()){
        qDebug(lgSPVerbose)<<"Failed to find value at key "<<key.c_str();
        return MaybeQVariantMapMeta();
    }

    auto [n,m,p] = val.value();

    const auto q = n.node();

    const toml::node_view<const toml::node> node =toml::node_view<const toml::node>(*q);
    auto meta= m;
    auto place = p;

    auto outVal = tomlNodeViewToQVariant(node);
    if(outVal.canConvert<QVariantMap>()){
        return MaybeQVariantMapMeta( {outVal.toMap(), meta, place} );
    }
    return MaybeQVariantMapMeta( {QVariantMap(), meta, place} );
}

}

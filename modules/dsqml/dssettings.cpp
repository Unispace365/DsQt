#include "dssettings.h"
#include <filesystem>
#include <iostream>
#include <string>
#include <toml++/toml.h>
#include "dsenvironment.h"

namespace dsqt{

std::string DSSettings::mConfigurationDirectory="";
bool DSSettings::mLoadedConfiguration = false;
std::unordered_map<std::string,DSSettingsRef> DSSettings::sSettings;
std::unordered_map<std::string,std::function<void(QColor&,float,float,float,float,float)>>
    DSSettings::sColorConversionFuncs = {
{"float_rgb",[](QColor& resultcolor,float v1,float v2,float v3,float v4,float v5){
    resultcolor.setRgbF(v1,v2,v3,v4);
}}
,{"int_rgb",[](QColor& resultcolor,float v1,float v2,float v3,float v4,float v5){
    resultcolor.setRgb((int)v1,(int)v2,(int)v3,(int)v4);
}}
,{"float_hsv",[](QColor& resultcolor,float v1,float v2,float v3,float v4,float v5){
    resultcolor.setHsvF(v1,v2,v3,v4);
}}
,{"int_hsv",[](QColor& resultcolor,float v1,float v2,float v3,float v4,float v5){
    resultcolor.setHsv((int)v1,(int)v2,(int)v3,(int)v4);
}}
,{"float_hsl",[](QColor& resultcolor,float v1,float v2,float v3,float v4,float v5){
    resultcolor.setHslF(v1,v2,v3,v4);
}}
,{"int_hsl",[](QColor& resultcolor,float v1,float v2,float v3,float v4,float v5){
    resultcolor.setHsl((int)v1,(int)v2,(int)v3,(int)v4);
}}
,{"float_cmyk",[](QColor& resultcolor,float v1,float v2,float v3,float v4,float v5){
    resultcolor.setCmykF(v1,v2,v3,v4,v5);
}}
,{"int_cmyk",[](QColor& resultcolor,float v1,float v2,float v3,float v4,float v5){
    resultcolor.setCmyk((int)v1,(int)v2,(int)v3,(int)v4,(int)v5);
}}
};

DSSettings::DSSettings(std::string name,QObject* parent):QObject(parent)
{
    mName = name;
}

DSSettings::~DSSettings(){

}

bool DSSettings::loadSettingFile(const std::string &file)
{

    std::string fullFile = DSEnvironment::expand(file);
    if(!std::filesystem::exists(fullFile)){
        qWarning()<<"File doesn't exist warning: Attempting to load file \""<<fullFile.c_str()<<"\" but it does not exist";
        return false;
    }
    auto clearItr = mResultStack.end();
     SettingFile* loadResult = nullptr;
    for(auto resultItr = mResultStack.begin();resultItr != mResultStack.end();++resultItr){
        if(resultItr->filepath == fullFile){
            qWarning()<<"File already loaded warning: Updating already loaded file";
            loadResult = &(*resultItr);
        }
    }

    if(!loadResult){
        mResultStack.push_back(SettingFile());
        loadResult  = &mResultStack.back();
    }

    loadResult->filepath = fullFile;
    loadResult->valid = false;
    try {
        loadResult->data = toml::parse_file(fullFile);
        loadResult->valid=true;
    } catch (const toml::parse_error& e){
        qWarning() << "Faild to parse setting file \""<<fullFile.c_str()<<"\n:"<<e.what();
        return false;
    }

    return true;

}



std::tuple<bool,DSSettingsRef> DSSettings::getSettingsOrCreate(const std::string &name, QObject* parent) {

    if(sSettings.find(name) != sSettings.end()){
        return {true,sSettings[name]};
    }

    DSSettingsRef retVal = DSSettingsRef(new DSSettings(name,parent));
    return {false,retVal};
}

DSSettingsRef DSSettings::getSettings(const std::string &name) {

    if(sSettings.find(name) == sSettings.end()){
        return DSSettingsRef(nullptr);
    }

    return sSettings[name];
}

//template<class T> std::optional<T> DSSettings::getValue(const std::string& key){
//     auto retval = getWithMeta<T>(key);
//     if(retval){
//        auto [value,x,y] = retval.value();
//        return value;
//     }
//     return std::optional<T>();
//}

//template<> std::optional<std::string> DSSettings::getValue(const std::string& key){
//     auto retval = getWithMeta<std::string>(key);
//     if(retval){
//        auto [value,x,y] = retval.value();
//        return value;
//     }
//     return std::optional<std::string>();
//}

template<class T> T DSSettings::getOr(const std::string& key,const T& def){
     std::optional<T> val = getValue<T>(key);
     if(val.has_value()) return val.value();
     return def;
}

template<> std::optional<ValueWMeta<QColor>> DSSettings::getWithMeta(const std::string& key){
    auto val = getNodeViewWithMeta(key);
    if(!val.has_value()) return std::optional<ValueWMeta<QColor>>();

    auto [node,meta,place] = val.value();
    QColor resultcolor;
    auto& metaData = *meta;
    std::string elementType = metaData["element_type"].value_or<std::string>("float");
    std::string colorType = metaData["array_color_type"].value_or<std::string>("rgb");
    std::function<void(float v1,float v2,float v3,float v4,float v5)> setColor;
    std::string functionSelector = elementType+"_"+colorType;

    if(node.is_array()){
        toml::array* array = node.as_array();
        float r= array[0].value_or(0);
        float g= array[1].value_or(0);
        float b= array[2].value_or(0);
        float a= array[3].value_or(1);
        float a2= array[4].value_or(1);
        //array of 1 is a solid gray.
        if(array->size()==1){
            sColorConversionFuncs[elementType+"_rgb"](resultcolor,r,r,r,a,0);
        } else if(array->size()==2){
            sColorConversionFuncs[elementType+"_rgb"](resultcolor,r,r,r,g,0);
        } else if(array->size()>=3 && array->size()>=5){
            sColorConversionFuncs[functionSelector](resultcolor,r,g,b,a,a2);
        }  else {
            resultcolor.setRgb(0,0,0,1);
        }
    } else if (node.is_table()) {
        toml::table& table = *node.as_table();
        //float v1,v2,v3,v4,v5=0;

        float r = table["r"].value_or(0);
        float g = table["g"].value_or(0);
        float b = table["b"].value_or(0);

        float h = table["h"].value_or(0);
        float s = table["s"].value_or(0);
        float v = table["v"].value_or(0);

        float l = table["l"].value_or(0);

        float c = table["c"].value_or(0);
        float m = table["m"].value_or(0);
        float y = table["y"].value_or(0);
        float k = table["k"].value_or(0);

        float a = table["a"].value_or(1);
        if(table.contains("h") && table.contains("s") && table.contains("l") ){
            sColorConversionFuncs[elementType+"_hsl"](resultcolor,h,s,l,a,0);
        } else
        if(table.contains("h") && table.contains("s") && table.contains("v") ){
            sColorConversionFuncs[elementType+"_hsv"](resultcolor,h,s,v,a,0);
        } else
        if(table.contains("r") && table.contains("g") && table.contains("b") ){
            sColorConversionFuncs[elementType+"_rgb"](resultcolor,r,g,b,a,0);
        } else
        if(table.contains("c") && table.contains("m") && table.contains("y") && table.contains("k") ){
            sColorConversionFuncs[elementType+"_rgb"](resultcolor,c,m,y,k,a);
        } else
        {
            resultcolor.setRgbF(0,0,0,1.0f);
        }

    } else if (node.is_string()) {
        std::string val = node.value_or<std::string>("");
        if(QColor::isValidColorName(val)) {
            resultcolor = QColor::fromString(val);
        } else {
            resultcolor.setRgbF(0,0,0,1.0f);
        }
    }
     return std::optional<ValueWMeta<QColor>>({resultcolor,meta,place});

}

std::optional<NodeWMeta> DSSettings::getNodeViewWithMeta(const std::string& key){
    std::string returnPath = "";

    if(mRuntimeResult.data.contains(key)){
        mRuntimeResult.filepath="runtime";
        returnPath = mRuntimeResult.filepath;
        toml::table* metaTable = nullptr;
        toml::node_view value = toml::node_view<toml::node>();
        const auto& nodeView = mRuntimeResult.data.at_path(key);

        if(nodeView.is_array() && nodeView.as_array()->size()>0){
            value = toml::node_view<toml::node>(nodeView.as_array()->at(0));
        }
        if(nodeView.is_array() && nodeView.as_array()->size()>1 && nodeView.as_array()->at(1).is_table()){
            metaTable = nodeView.as_array()->at(1).as_table();
        }
        return std::optional(NodeWMeta({value,metaTable,returnPath}));
    }

    for(auto iter = mResultStack.rbegin();iter != mResultStack.rend();++iter){
        auto& stackItem = *iter;
        if(key==std::string("config_folder")){
            std::stringstream ss;
            ss << stackItem.data;
            qDebug() << ss.str().c_str();
        }
        if(stackItem.data.at_path(key)){
            returnPath = stackItem.filepath;
            toml::table* metaTable = nullptr;
            toml::node_view value = toml::node_view<toml::node>();
            const auto& nodeView = stackItem.data.at_path(key);
            value = nodeView;
            if(nodeView.is_array() && nodeView.as_array()->size()>0){
                value = toml::node_view<toml::node>(nodeView.as_array()->at(0));
            }
            if(nodeView.is_array() && nodeView.as_array()->size()>1 && nodeView.as_array()->at(1).is_table()){
                metaTable = nodeView.as_array()->at(1).as_table();
            }
            return std::optional(NodeWMeta({value,metaTable,returnPath}));
        }
    }
    return std::optional<NodeWMeta>(); //{toml::node_view<toml::node>(),nullptr,""};
}



template<typename T> void DSSettings::set(std::string& key, T &value){
    toml::path p(key);
    //build any missing parts
    auto iter = p.begin();
    for(iter = p.begin();iter!=p.end();++iter){
        if(true){}
    }
}



}//namespace dsqt

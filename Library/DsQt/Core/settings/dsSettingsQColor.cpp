#include "settings/dsSettings.h"

namespace dsqt {

std::unordered_map<std::string,std::function<void(QColor&,float,float,float,float,float)>>
    DsSettings::sColorConversionFuncs = {
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

template<>
MaybeQColorMeta DsSettings::getWithMeta(const std::string& key){
    auto val = getNodeViewWithMeta(key);
    if(!val.has_value()) return MaybeQColorMeta();

    auto [node, meta, place] = val.value();
    QColor resultcolor;

    std::string elementType = "float";
    std::string colorType = "rgb";
    if(meta){
        auto& metaData = *meta;
        elementType = metaData["element_type"].value_or<std::string>("float");
        colorType = metaData["array_color_type"].value_or<std::string>("rgb");
    }

    float defMax = elementType=="float"?1:255;
    std::function<void(float v1,float v2,float v3,float v4,float v5)> setColor;
    std::string functionSelector = elementType+"_"+colorType;

    if(node.is_array()){
        toml::array& array = *(node.as_array());
        float r= array.size()>0?array[0].value_or<double>(0):0.0;
        float g= array.size()>1?array[1].value_or<double>(0):0;
        float b= array.size()>2?array[2].value_or<double>(0):0;
        float a= array.size()>3?array[3].value_or<double>(1):colorType=="cmyk"?0:defMax;
        float a2= array.size()>4?array[4].value_or<double>(1):defMax;
        //array of 1 is a solid gray.
        if(array.size()==1){
            sColorConversionFuncs[elementType+"_rgb"](resultcolor,r,r,r,a,0);
        } else if(array.size()==2){
            sColorConversionFuncs[elementType+"_rgb"](resultcolor,r,r,r,g,0);
        } else if(array.size()>=3 && array.size()<=5){
            sColorConversionFuncs[functionSelector](resultcolor,r,g,b,a,a2);
        }  else {
            resultcolor.setRgb(0,0,0,1);
        }
    } else if (node.is_table()) {
        toml::table& table = *(node.as_table());
        //float v1,v2,v3,v4,v5=0;

        float r = table["r"].value_or<double>(0);
        float g = table["g"].value_or<double>(0);
        float b = table["b"].value_or<double>(0);

        float h = table["h"].value_or<double>(0);
        float s = table["s"].value_or<double>(0);
        float v = table["v"].value_or<double>(0);

        float l = table["l"].value_or<double>(0);

        float c = table["c"].value_or<double>(0);
        float m = table["m"].value_or<double>(0);
        float y = table["y"].value_or<double>(0);
        float k = table["k"].value_or<double>(0);

        float a = table["a"].value_or<double>(defMax);
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
                        sColorConversionFuncs[elementType+"_cmyk"](resultcolor,c,m,y,k,a);
                    } else
                    {
                        resultcolor.setRgbF(0,0,0,1.0f);
                    }

    } else if (node.is_string()) {
        std::string strVal = node.value_or<std::string>("");
        if(QColor::isValidColorName(strVal)) {
            resultcolor = QColor::fromString(strVal);
        } else {
            if(strVal !=""){
                auto col = get<QColor>(strVal).value();
                if(col.isValid()){
                    resultcolor = col;
                } else {
                    qCWarning(lgSettingsParser)<<"Color string \""<<strVal.c_str()<<"\" is not a valid color name or hex value.";
                    return MaybeQColorMeta();
                }
            }
            resultcolor.setRgbF(0,0,0,1.0f);
        }
    }
    return MaybeQColorMeta( {resultcolor, meta, place} );

}
}

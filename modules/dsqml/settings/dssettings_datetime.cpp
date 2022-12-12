#include <dssettings.h>
#include <string>
#include <algorithm>
#include <QDate>
#include <QTime>
#include <QDateTime>

namespace dsqt {

std::optional<toml::date_time> getDateTimeFromNode(toml::node_view<toml::node>& node_view){
    std::optional<toml::date_time> retval;
    if(node_view.is_date_time()){
        retval = node_view.value<toml::date_time>();
    } else if(node_view.is_date()){
        const toml::date date = (node_view.value<toml::date>().value());
        retval = std::optional<toml::date_time>(toml::date_time(date));
    } else if(node_view.is_time()){
        const toml::time time = (node_view.value<toml::time>().value());
        retval = std::optional<toml::date_time>(toml::date_time(time));
    }
    return retval;
};

template<> std::optional<ValueWMeta<QDate>> DSSettings::getWithMeta(const std::string& key){

    auto val = getNodeViewWithMeta(key);
    if(!val.has_value()){
        qDebug(settingsParser)<<"Failed to find value at key "<<key.c_str();
        return std::optional<ValueWMeta<QDate>>();
    }

    auto [n,m,p] = val.value();
    auto node =n;
    auto meta= m;
    auto place = p;
    toml::date_time toml_datetime;
    QDate retval;
    if(!node.is_string()){
        auto datetime_node = getDateTimeFromNode(node);
        if(datetime_node){
            auto date = datetime_node.value().date;
            retval.setDate(date.year,date.month,date.day);
        }
    } else if(node.is_string()) {
        auto string = node.value<std::string>().value();
        if(mCustomDateFormat.isEmpty()){
            retval = QDate::fromString(QString::fromStdString(string),mDateFormat);
        } else {
            retval = QDate::fromString(QString::fromStdString(string),mCustomDateFormat);
        }
    }

    return std::optional<ValueWMeta<QDate>>(ValueWMeta<QDate>(retval,meta,place));;

}

template<> std::optional<ValueWMeta<QTime>> DSSettings::getWithMeta(const std::string& key){

    auto val = getNodeViewWithMeta(key);
    if(!val.has_value()){
        qDebug(settingsParser)<<"Failed to find value at key "<<key.c_str();
        return std::optional<ValueWMeta<QTime>>();
    }

    auto [n,m,p] = val.value();
    auto node =n;
    auto meta= m;
    auto place = p;
    toml::date_time toml_datetime;
    QTime retval;
    if(!node.is_string()){
        auto datetime_node = getDateTimeFromNode(node);
        if(datetime_node){
            auto time = datetime_node.value().time;
			if(!datetime_node.value().is_local()){
				qCWarning(settingsParser)<<"The time setting has an offset but QTime won't capture it. Try using QDateTime.";
			}
			retval.setHMS(time.hour,time.minute,time.second,time.nanosecond/1000000);
        }
    } else if(node.is_string()) {
        auto string = node.value<std::string>().value();
        if(mCustomDateFormat.isEmpty()){
            retval = QTime::fromString(QString::fromStdString(string),mDateFormat);
        } else {
            retval = QTime::fromString(QString::fromStdString(string),mCustomDateFormat);
        }
    }

    return std::optional<ValueWMeta<QTime>>(ValueWMeta<QTime>(retval,meta,place));;

}

template<> std::optional<ValueWMeta<QDateTime>> DSSettings::getWithMeta(const std::string& key){

    auto val = getNodeViewWithMeta(key);
    if(!val.has_value()){
        qDebug(settingsParser)<<"Failed to find value at key "<<key.c_str();
        return std::optional<ValueWMeta<QDateTime>>();
    }

    auto [n,m,p] = val.value();
    auto node =n;
    auto meta= m;
    auto place = p;
    toml::date_time toml_datetime;
    QDateTime retval;
    if(!node.is_string()){
        auto datetime_node = getDateTimeFromNode(node);
        if(datetime_node){
            auto date = datetime_node.value().date;
            auto time = datetime_node.value().time;
            auto datetime = datetime_node.value();
            QDate qDate(date.year,date.month,date.day);
			QTime qTime(time.hour,time.minute,time.second,time.nanosecond/1000000);
            retval.setDate(qDate);
            retval.setTime(qTime);
            if(!datetime.is_local()){
                retval.setOffsetFromUtc(datetime.offset.value().minutes*60);
            }
        }
    } else if(node.is_string()) {
        auto string = node.value<std::string>().value();
        if(mCustomDateFormat.isEmpty()){
            retval = QDateTime::fromString(QString::fromStdString(string),mDateFormat);
        } else {
            retval = QDateTime::fromString(QString::fromStdString(string),mCustomDateFormat);
        }
    }

    return std::optional<ValueWMeta<QDateTime>>(ValueWMeta<QDateTime>(retval,meta,place));;

}

}

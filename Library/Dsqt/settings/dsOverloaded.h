#ifndef DSOVERLOADED_H
#define DSOVERLOADED_H

#include <string>
#include <optional>
#include <toml++/toml.h>

namespace dsqt {

template <class... Ts> struct DSOverloaded : Ts... {
    using Ts::operator()...;
    //template<class T> const std::optional<T> operator()(toml::value<> v){return std::optional(v.get());}
};
template <class... Ts> DSOverloaded(Ts...) -> DSOverloaded<Ts...>;

template<class T,class S>
struct BaseConversion {
    const std::optional<T> operator()(S v){
       return std::nullopt;
    }
    const std::optional<T> operator()(toml::value<std::string> v){
        return std::nullopt;
    };
    const std::optional<T> operator()(toml::value<int64_t> v){
        return std::nullopt;
    };
    const std::optional<T> operator()(toml::value<double> v){
        return std::nullopt;
    };
    const std::optional<T> operator()(toml::value<bool> v){
        return std::nullopt;
    };
    const std::optional<T> operator()(toml::array v){
        return std::nullopt;
    };
    const std::optional<T> operator()(toml::table v){
        return std::nullopt;
    };

    const std::optional<T> operator()(toml::date v){
        return std::nullopt;
    };

    const std::optional<T> operator()(toml::time v){
        return std::nullopt;
    };

    const std::optional<T> operator()(toml::date_time v){
        return std::nullopt;
    };
};

template<class S> struct BaseConversion<std::string, S>{
    const std::optional<std::string> operator()(toml::value<std::string> v){
        return std::optional(v.get());
    };
    const std::optional<std::string> operator()(toml::value<int64_t> v){
        std::stringstream ss;
        ss << v.get();
        return std::optional<std::string>(ss.str());
    };
    const std::optional<std::string> operator()(toml::value<double> v){
        std::stringstream ss;
        ss << (v.get());
        return std::optional<std::string>(ss.str());
    };
    const std::optional<std::string> operator()(toml::value<bool> v){
        return v.get()?std::optional("true"):std::optional("false");
    };

    const std::optional<std::string> operator()(toml::date v){
        std::stringstream ss;
        ss << v.year << "-" << v.month << "-" << v.day;
        return std::optional(ss.str());
    };

    const std::optional<std::string> operator()(toml::time v){
        std::stringstream ss;
        ss <<"T"<< v.hour << ":" << v.minute << ":" << v.second << "." <<v.nanosecond;
        return std::optional(ss.str());
    };

    const std::optional<std::string> operator()(toml::date_time v){
        std::stringstream ss;

        auto dt = v.date;
        auto tm = v.time;
        ss << dt.year << "-" << dt.month << "-" << dt.day;
        ss <<"T"<< tm.hour << ":" << tm.minute << ":" << tm.second << "." <<tm.nanosecond;
        if(!v.is_local()){
            auto off = v.offset.value_or(toml::time_offset(0,0));
            int hrs = std::abs(off.minutes)/60;
            int mns = std::abs(off.minutes) - hrs*60;
            ss << (off.minutes>=0?"+":"-") << hrs << ":" <<mns;
        }
        return std::optional(ss.str());
    };
};


}//namespace dsqt
#endif // DSOVERLOADED_H

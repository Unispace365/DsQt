#include "dscontexthash.h"

std::hash<std::string> hash;
dsContext operator "" _ctx(const char* c, size_t t){
    std::string value(c,t);
    return hash(value);
}

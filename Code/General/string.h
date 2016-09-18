//--------------------------------------------------------------
// Copyright © 2015 Rimmer Motzheim - All rights reserved
//--------------------------------------------------------------

#ifndef Tritium_Core_String_h
#define Tritium_Core_String_h

#include "types.h"
#include <string>

template<class F>
auto map(F f, const std::string& string) {
    std::string target;
    target.reserve(string.length());

    for(Size i = 0; i < string.length(); i++) {target.push_back(f(string[i]));}
    return move(target);
}

template<class F>
void walk(F f, const std::string& string) {
    for(Size i = 0; i < string.length(); i++) {f(i);}
}

template<class T, class F>
void modify(F f, std::string& string) {
    for(Size i = 0; i < string.length(); i++) {f(i);}
}

template<class T, class U, class F>
auto fold(F f, U start, T* begin, T* end) -> decltype(f(*begin, start)) {
    if(begin != end) {
        auto next = begin; ++next;
        return fold(f, f(*begin, start), next, end);
    }
    else return start;
}

template<class U, class F>
auto fold(F f, U start, const std::string& list) {
    return fold(f, start, list.c_str(), list.c_str() + list.length());
}

#endif // Tritium_Core_String_h
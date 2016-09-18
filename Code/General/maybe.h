//--------------------------------------------------------------
// Copyright © 2015 Rimmer Motzheim - All rights reserved
//--------------------------------------------------------------

#ifndef Tritium_Core_Maybe_h
#define Tritium_Core_Maybe_h

#include "types.h"

template<class T> struct Uninitialized {
    template<class... P> void init(P&&... p) {
        new (data) T(forward<P>(p)...);
    }

    T* operator -> () {return (T*)data;}
    operator T& () {return *(T*)data;}
    T* operator & () {return (T*)data;}
    T& operator * () {return *(T*)data;}

    const T* operator -> () const {return (T*)data;}
    operator const T& () const {return *(T*)data;}
    const T* operator & () const {return (T*)data;}
    const T& operator * () const {return *(T*)data;}

private:
    Size data[(sizeof(T) + sizeof(Size) - 1) / sizeof(Size)];
};

struct MaybeNothing {};

template<class T> struct Maybe {
    Uninitialized<T> val;
    bool just;

    explicit Maybe(T&& t) : just(true) {val.init(move(t));}
    explicit Maybe(const T& t) : just(true) {val.init(t);}
    Maybe(MaybeNothing) : just(false) {}

    bool isJust() const {return just;}
    bool isNothing() const {return !just;}
    T& force() {return *val;}
    const T& force() const {return *val;}
    T* get() {
        if(just) return &val;
        else return nullptr;
    }

    const T* get() const {
        if(just) return &val;
        else return nullptr;
    }

    operator bool () const {return just;}
    T from(const T& a) const {return just ? val : a;}
    T from(T&& a) const {return just ? val : move(a);}
    template<class U, class F> U maybe(const U& u, F f) const {return just ? f(force()) : u;}
    template<class U, class F> U maybe(U&& u, F f) const {return just ? f(force()) : move(u);}
};

template<class T> struct Maybe<T*> {
    T* val;

    explicit Maybe(T* t) : val(t) {}
    explicit Maybe() : val(nullptr) {}
    Maybe(MaybeNothing) : val(nullptr) {}

    bool isJust() const {return val != nullptr;}
    bool isNothing() const {return val == nullptr;}
    T* force() const {return val;}
    T* get() const {return val;}

    operator bool () {return val != nullptr;}
    T* from(T* a) const {return val ? val : a;}
    template<class U, class F> U* maybe(U* u, F f) {return val ? f(val) : u;}
};

template<class T> auto Just(T&& t) {return Maybe<typename removeReference<T>::type>(forward<T>(t));}
inline MaybeNothing Nothing() {return MaybeNothing{};}

template<class T, class F> auto operator | (const Maybe<T>& a, F f) -> decltype(f(a.force())) {
    if(a.isJust()) return f(a.force());
    else return Nothing();
}

#endif // Tritium_Core_Maybe_h
//--------------------------------------------------------------
// Copyright © 2015 Rimmer Motzheim - All rights reserved
//--------------------------------------------------------------

#ifndef Tritium_Core_Base_h
#define Tritium_Core_Base_h

#include "targets.h"
#include <utility>

/*
 * Base types.
 */

using U8 = unsigned char;
using I8 = char;
using U16 = unsigned short;
using I16 = short;
using U32 = unsigned int;
using I32 = int;
using F32 = float;
using F64 = double;
using Float = float;

using Byte = unsigned char;
using Bool = bool;

using Id = U32;

#ifdef __X64__
#ifdef __WINDOWS__
using Size = unsigned long long;
using Int = long long;
#else //__WINDOWS__
using Size = unsigned long;
using Int = long;
#endif //__WINDOWS__
using I64 = Int;
using U64 = Size;
#else //__X64__
using Size = unsigned int;
using Int = int;
using I64 = long long;
using U64 = unsigned long long;
#endif //__X64__

using Char = char;
using Nullptr = decltype(nullptr);
using Handle = void*;

#ifdef __WINDOWS__
using WChar = wchar_t;
using WChar32 = I32;
#endif

#ifdef __POSIX__
using WChar = I16;
using WChar32 = wchar_t;
#endif

// initializer list implementation.
// This needs to be a separate file with this exact name in some compilers.
#include "initializer_list"

#ifdef _MSC_VER
#define forceinline __forceinline
#define noinline __declspec(noinline)
#define THREAD_LOCAL __declspec(thread)
#else
#define forceinline inline __attribute__ ((always_inline))
#define noinline __attribute__ ((noinline))
#define THREAD_LOCAL __thread
#endif

/*
 * Template stuff.
 */

template<class T, T Val> struct integralConstant {
    static constexpr const T value = Val;
    using type = integralConstant;
    constexpr operator T() const {return Val;}
};

using trueConstant = integralConstant<bool, true>;
using falseConstant = integralConstant<bool, false>;

template<class T> struct isLReference : falseConstant {};
template<class T> struct isLReference<T&> : trueConstant {};

template<class T> struct removeConst {typedef T type;};
template<class T> struct removeConst<const T> {typedef T type;};
template<class T> struct removeConst<const T[]> {typedef T type[];};
template<class T, Size N> struct removeConst<const T[N]> {typedef T type[N];};

template<class T> struct removeVolatile {typedef T type;};
template<class T> struct removeVolatile<volatile T> {typedef T type;};
template<class T> struct removeVolatile<volatile T[]> {typedef T type[];};
template<class T, Size N> struct removeVolatile<volatile T[N]> {typedef T type[N];};

template<class T> struct removeCV {typedef typename removeConst<typename removeVolatile<T>::type>::type type;};

template<class T> struct removeReference {typedef T type;};
template<class T> struct removeReference<T&> {typedef T type;};
template<class T> struct removeReference<T&&> {typedef T type;};

template<class T> struct removeCVR {typedef typename removeReference<typename removeCV<T>::type>::type type;};

template<class T> struct removePointer {typedef T type;};
template<class T> struct removePointer<T*> {typedef T type;};
template<class T> struct removePointer<T* const> {typedef T type;};
template<class T> struct removePointer<T* volatile> {typedef T type;};
template<class T> struct removePointer<T* const volatile> {typedef T type;};

template<class T> struct removeExtent {typedef T type;};
template<class T> struct removeExtent<T[]> {typedef T type;};
template<class T, Size N> struct removeExtent<T[N]> {typedef T type;};

template<class T> struct removeAllExtents {typedef T type;};
template<class T> struct removeAllExtents<T[]> {typedef typename removeAllExtents<T>::type type;};
template<class T, Size N> struct removeAllExtents<T[N]> {typedef typename removeAllExtents<T>::type type;};

template<class T> struct addConst {typedef const T type;};
template<class T> struct addVolatile {typedef volatile T type;};
template<class T> struct addCV {typedef const volatile T type;};

template<class T> struct addReference {typedef T& type;};
template<> struct addReference<void> {typedef void type;};

template<class T> struct addRReference {typedef T&& type;};
template<> struct addRReference<void> {typedef void type;};

template<class T> struct addPointer {typedef typename removeReference<T>::type* type;};

template<class T>
typename addRReference<T>::type declVal();

template<class>
struct resultOf;

template<class F, class... Args>
struct resultOf<F(Args...)> {
    typedef decltype(declVal<F>()(declVal<Args>()...)) type;
};

template<class T1, class T2> struct isSame : falseConstant {};
template<class T> struct isSame<T, T> : trueConstant {};

namespace Internal {
// Helper struct to determine the alignment of T.
// By putting a char between two T types and comparing it to sizeof(T)*2, we can get the alignment.
template<typename T> struct IntGetAlignment {
    T o1;
    char pad;
    T o2;
};

//Returns the alignment of a struct (see IntGetAlignment for details).
#define __Traits_GetAlignment(T) (sizeof(Internal::IntGetAlignment<T>) - (2 * sizeof(T)))
} // namespace Internal

// Produces the alignment of a struct as template.
template<typename T> struct alignmentOf : integralConstant<Size, __Traits_GetAlignment(T)> {};

// We assume references are aligned in the same way as pointers.
template<typename T> struct alignmentOf<T&> : integralConstant<Size, __Traits_GetAlignment(T*)> {};

#undef __Traits_GetAlignment

template<class T> constexpr T&& forward(typename removeReference<T>::type& t) {return (T&&)t;}
template<class T> constexpr T&& forward(typename removeReference<T>::type&& t) {
    static_assert(!isLReference<T>::value, "Cannot forward an rvalue as an lvalue.");
    return (T&&)t;
}

template<class T> void swap(T& x, T& y) {
    T z(std::move(x));
    x = std::move(y);
    y = std::move(z);
}

/*
 * Type info
 */

template<typename T> struct hasSign { enum { value = 1 }; };
template<> struct hasSign<U64> { enum { value = 0 }; };
template<> struct hasSign<U32> { enum { value = 0 }; };
template<> struct hasSign<U16> { enum { value = 0 }; };
template<> struct hasSign<U8> { enum { value = 0 }; };

template<class T> struct maxLimit { enum : Byte { value = 0 }; };
template<> struct maxLimit<I8> { enum : I8 { value = 127 }; };
template<> struct maxLimit<U8> { enum : U8 { value = 255 }; };
template<> struct maxLimit<I16> { enum : I16 { value = 32767 }; };
template<> struct maxLimit<U16> { enum : U16 { value = 0xffff }; };
template<> struct maxLimit<I32> { enum : I32 { value = 2147483647 }; };
template<> struct maxLimit<U32> { enum : U32 { value = 0xffffffff }; };
template<> struct maxLimit<I64> { static const I64 value = 9223372036854775807; };
template<> struct maxLimit<U64> { static const U64 value = 0xffffffffffffffff; };

template<class T> struct minLimit { enum : Byte { value = 0 }; };
template<> struct minLimit<I8> { enum : I8 { value = -127 - 1 }; };
template<> struct minLimit<U8> { enum : U8 { value = 0 }; };
template<> struct minLimit<I16> { enum : I16 { value = -32767 - 1 }; };
template<> struct minLimit<U16> { enum : U16 { value = 0 }; };
template<> struct minLimit<I32> { enum : I32 { value = -2147483647 - 1 }; };
template<> struct minLimit<U32> { enum : U32 { value = 0 }; };
template<> struct minLimit<I64> { static const I64 value = -9223372036854775807 - 1; };
template<> struct minLimit<U64> { static const U64 value = 0; };

#endif // Tritium_Core_Base_h
//--------------------------------------------------------------
// Copyright © 2015 Rimmer Motzheim - All rights reserved
//--------------------------------------------------------------

#ifndef Tritium_Core_Mem_Hash_h
#define Tritium_Core_Mem_Hash_h

#include "types.h"

/**
 * Generates a human-readable string from a hash.
 * Out must be an array of at least 9 chars (number 9 will be set to 0).
 */
Char* stringFromHash(U32 hash, Char* out);
WChar* stringFromHash(U32 hash, WChar* out);

/**
 * Creates a 32-bit hash from various input data using an SBox-algorithm.
 * Used for hash tables, unique identifiers, etc. Do not use for secure hashing!
 */
struct Hasher {
    Hasher() = default;

    U32 get() const {return hash;}

    void addData(const void* data, Size count);
    void addString(const Char* string);
    void addString(const WChar* string);
    void addString(const WChar32* string);

    void add(U32);
    void add(I32);
    void add(F32);
    void add(F64);

    template<typename T> void add(const T& data) {addData(&data, sizeof(T));}

    explicit operator U32() const {return get();}

private:
    U32 hash = 0;
};

template<class T>
inline Hasher& operator << (Hasher& h, const T& data) {
    h.add(data);
    return h;
}

/// Default hash implementation for objects.
/// Can be specialized for specific types.
template<class T>
U32 hash(const T& t) {
    Hasher h;
    h.add(t);
    return (U32)h;
}

constexpr U32 staticHash(const char* string, Size size) {
    return size ? *string + 234972 * staticHash(string + 1, size - 1) : 570932437;
}

/// Compile-time string hash generation.
constexpr U32 operator "" _h(const char* string, Size size) {
    return staticHash(string, size);
}

#endif // Tritium_Core_Mem_Hash_h
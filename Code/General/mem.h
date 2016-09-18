
//--------------------------------------------------------------
// Copyright © 2015 Rimmer Motzheim - All rights reserved
//--------------------------------------------------------------

#ifndef Tritium_Mem_StaticBuffer_h
#define Tritium_Mem_StaticBuffer_h

#include "types.h"
#include <atomic>

namespace Tritium {

/**
 * Provides permanent memory allocation.
 * The buffer cannot be resized, due to multithreading issues.
 * However, the memory pages are not mapped before they are used, so it is save to use large sizes.
 */
struct StaticBuffer {
    StaticBuffer() = default;
    StaticBuffer(Size maxSize) {init(maxSize);}
    ~StaticBuffer() {destroy();}

    void init(Size maxSize);
    void* alloc(Size size);
    void destroy();

    template<class T, class... P>
    T* create(P&&... params) {
        T* x = (T*)alloc(sizeof(T));
        new ((void*)x) T(forward<P>(params)...);
        return x;
    }

    Size getUsed() {return usedSize;}
    Size getAllocations() {return allocations;}

    void clear() {
        usedSize = 0;
        allocations = 0;
    }

private:
    void* data = 0;
    Size maxSize = 0;
    std::atomic_size_t usedSize{0};
    std::atomic_size_t allocations{0};
};

} //namespace Tritium

// Allows creating objects in a static buffer like: new(buffer) Type(args);
inline void* operator new (Size count, Tritium::StaticBuffer& buffer) {return buffer.alloc(count);}

struct HeapAllocator {
    static void* alloc(Size size);
    static void* reAlloc(void* data, Size newSize);
    static void free(void* data);
};

U32 findLastBit(U32 a);

#endif // Tritium_Mem_StaticBuffer_h
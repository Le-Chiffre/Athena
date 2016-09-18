//--------------------------------------------------------------
// Copyright © 2015 Rimmer Motzheim - All rights reserved
//--------------------------------------------------------------

#include <cstdlib>
#include <cassert>
#include <cstring>
#include "mem.h"

namespace Tritium {

void StaticBuffer::init(Size maxSize) {
    destroy();
    data = malloc(maxSize);

#ifdef _DEBUG
    assert(data != 0);
    memset(data, 0xcdcdcdcd, maxSize);
#endif

    this->maxSize = maxSize;
    this->usedSize = 0;
    this->allocations = 0;
}

void* StaticBuffer::alloc(Size size) {
    assert(data != 0);

    // We align all allocations to 16 bytes, so we don't have to worry about SIMD alignment.
    size = (size + 15) / 16 * 16;
    auto newSize = usedSize += size;
    if(newSize > maxSize) {
        assert("The static buffer has run out of space. You need to increase its size." == nullptr);

        // Allocate from the heap as a fallback.
        return malloc(size);
    }

    allocations++;
    assert(newSize - size % 4 == 0);
    return (Byte*)data + newSize - size;
}

void StaticBuffer::destroy() {
    if(data) {
        clear();
        free(data);
        data = nullptr;
        maxSize = 0;
    }
}

} //namespace Tritium

void* HeapAllocator::alloc(Size size) {return malloc(size);}
void* HeapAllocator::reAlloc(void* data, Size newSize) {return realloc(data, newSize);}
void HeapAllocator::free(void* data) {return ::free(data);}

U32 findLastBit(U32 a) {
#ifdef __GNUC__
    return sizeof(a)*8 - 1 - __builtin_clz(a);
#elif defined(_MSC_VER)
    unsigned long pos;
#ifdef __X64__
    _BitScanReverse64(&pos, a);
#else
	_BitScanReverse(&pos, a);
#endif
	return sizeof(a)*8 - 1 - pos;
#else
	//Very naive implementation.
	Size c = sizeof(a)*8 - 1;
    const Size mask = 1 << c;
	while(!(a & mask)) {
		a <<= 1;
		c--;
	}
	return c;
#endif
}
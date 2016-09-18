//--------------------------------------------------------------
// Copyright © 2015 Rimmer Motzheim - All rights reserved
//--------------------------------------------------------------

#ifndef Tritium_Core_Mem_Pool_h
#define Tritium_Core_Mem_Pool_h

#include <cassert>
#include <cstring>
#include "types.h"
#include "mem.h"

/**
 * Base class for all pool allocators.
 * Provides functions that are independent of the allocation method.
 */
struct BasePool {
    struct Node {
        Node* next;
        Byte* data() {return (Byte*)this;}
    };

    /// Calls the provided object's destructor and frees its memory.
    template<typename T>
    void destroy(T* obj) {
        obj->~T();
        free(obj);
    }

    /**
     * Marks the block of memory at the provided address as being free.
     * @param obj The address to free. This must be an address that was returned by Alloc().
     */
    void free(void* obj) {
        auto node = (Node*)obj;
        node->next = mFree;
        mFree = node;
    }

protected:

    /**
     * Creates a free list for the provided memory range.
     * @param node The starting address of the range.
     * @param blockSize The size of a single block.
     * @param count The number of blocks to add to the list. Must be at least 1.
     */
    static void createFreeList(Node* node, Size blockSize, Size count) {
        // Create a linked list between all the nodes.
        for(Size i = 1; i < count; i++) {
            auto next = (Node*)((Byte*)node + blockSize);
            node->next = next;
            node = next;
        }
        node->next = nullptr;
    }

    Node* mFree = nullptr;
};


/**
 * Base class for pool allocators.
 * Provides a generic implementation of Alloc(),
 * which uses the function Commit() from a derived allocator.
 */
template<typename Derived>
struct TBasePool : BasePool {
    /**
     * Allocates an object of type T and calls its constructor.
     * The block size of this pool must be >= the size of T.
     * @param blockSize The block size this pool was created with.
     * @param p Parameters for the object's constructor.
     * @return The created object.
     */
    template<typename T, typename... P>
    T* create(Size blockSize, P&&... p) {
        assert(sizeof(T) <= blockSize);
        auto obj = (T*)alloc(blockSize);
        new (obj) T(forward<P>(p)...);
        return obj;
    }

    /**
     * Allocates a block of memory of the provided size.
     * @param blockSize The size of a single block.
     * This must be the same as the size that was provided to the constructor.
     * @return A pointer to the allocated memory. Returns nullptr when the pool is full.
     */
    void* alloc(Size blockSize) {
        if(!mFree) mFree = ((Derived*)this)->commit(blockSize);
        auto obj = mFree->data();
        mFree = mFree->next;

#ifdef _DEBUG
        memset(obj, 0xcdcdcdcd, blockSize);
#endif
        return obj;
    }
};

/**
 * Defines a pool allocator which uses a Core allocator type to allocate memory.
 */
template<typename Allocator>
struct BasePool_Alloc : TBasePool<BasePool_Alloc<Allocator>> {
    struct Block {
        void* operator new (Size, U32 size) {
            return Allocator::alloc(size + sizeof(Block));
        }

        void operator delete (void* data) {
            Allocator::free(data);
        }

        Block* next = nullptr;
        void* pad; // To allow simd types in pools.
    };

    /**
     * Creates a pool allocator with an external allocator.
     * @param blockSize The size of a single object in the pool.
     * @param count The initial number of objects in the pool.
     */
    BasePool_Alloc(Size blockSize, Size count) {
        if(count) {
            mBlocks = new (blockSize * count) Block;
            mLastBlock = mBlocks;
            BasePool::mFree = (BasePool::Node*)(mBlocks + 1);
            BasePool::createFreeList((BasePool::Node*)(mBlocks + 1), blockSize, count);
            mCount = (U32)count;
        }
    }

    /**
     * Frees all memory that was allocated by the pool.
     * Since this doesn't call any destructors,
     * you need to make sure to free all allocated non-POD objects manually.
     */
    ~BasePool_Alloc() {
        auto b = mBlocks;
        while(b) {
            auto p = b;
            b = b->next;
            delete p;
        }
    }

private:

    /**
     * Commits a memory range and creates a free list for it.
     * The size of the new range is calculated from the previous size.
     * @param blockSize The size of a single block.
     * @return A pointer to the first block in the newly committed range.
      */
    BasePool::Node* commit(Size blockSize) {
        // Allocate a memory block of twice the current size, and create a free list for it.
        auto count = mCount * 2 > 4U ? mCount * 2 : 4U;
        auto b = new (blockSize * count) Block;
        BasePool::createFreeList((BasePool::Node*)(b + 1), blockSize, count);
        mCount += count;

        // Add the created block to the list.
        if(mLastBlock) mLastBlock->next = b;
        else mBlocks = b;

        mLastBlock = b;
        return (BasePool::Node*)(b + 1);
    }

    friend TBasePool<BasePool_Alloc<Allocator>>;

    //---------------------------------------------------------

    Block* mBlocks = nullptr;
    Block* mLastBlock = nullptr;
    U32 mCount = 0;
};


/**
 * Defines a pool allocator which uses a Core allocator instance to allocate memory.
 */
template<typename Allocator>
struct BasePool_Inst : TBasePool<BasePool_Inst<Allocator>> {
    struct Block {
        void* operator new (Size, Allocator& alloc, U32 size) {
            return alloc.alloc(size + sizeof(Block));
        }

        static void destroy(Block* data, Allocator& alloc) {
            data->~Block();
            alloc.free(data);
        }

        Block* next = nullptr;
        BasePool::Node nodes[];
    };

    /**
     * Creates a pool allocator with an external allocator.
     * @param blockSize The size of a single object in the pool.
     * @param count The initial number of objects in the pool.
     */
    BasePool_Inst(Size blockSize, U32 count, Allocator& a) : mAlloc(a) {
        if(count) {
            mBlocks = new (mAlloc, blockSize * count) Block;
            mLastBlock = mBlocks;
            BasePool::mFree = mBlocks->nodes;
            BasePool::createFreeList(mBlocks->nodes, blockSize, count);
            mCount = count;
        }
    }

    /**
     * Frees all memory that was allocated by the pool.
     * Since this doesn't call any destructors,
     * you need to make sure to free all allocated non-POD objects manually.
     */
    ~BasePool_Inst() {
        auto b = mBlocks;
        while(b) {
            auto p = b;
            b = b->next;
            Block::destroy(p, mAlloc);
        }
    }

private:

    /**
     * Commits a memory range and creates a free list for it.
     * The size of the new range is calculated from the previous size.
     * @param blockSize The size of a single block.
     * @return A pointer to the first block in the newly committed range.
     */
    BasePool::Node* commit(Size blockSize) {
        // Allocate a memory block of twice the current size, and create a free list for it.
        auto count = mCount * 2 > 4U ? mCount * 2 : 4U;
        auto b = new (mAlloc, blockSize * count) Block;
        BasePool::createFreeList(b->nodes, blockSize, count);
        mCount += count;

        // Add the created block to the list.
        if(mLastBlock) mLastBlock->next = b;
        else mBlocks = b;

        mLastBlock = b;
        return b->nodes;
    }

    friend TBasePool<BasePool_Inst<Allocator>>;

    //---------------------------------------------------------

    Block* mBlocks = nullptr;
    Block* mLastBlock = nullptr;
    Allocator& mAlloc;
    U32 mCount = 0;
};

/**
 * Defines a pool allocator that allocates objects of a pre-determined type.
 */
template<typename T, typename PoolType>
struct Pool_Object : PoolType {
    struct Node {
        union {
            Byte object[sizeof(T)];
            Node* next;
        };
    };

    template<typename... P>
    Pool_Object(P... p) : PoolType{sizeof(Node), p...} {}

    template<typename... P>
    T* create(P&&... p) {
        return PoolType::template create<T>(sizeof(Node), forward<P>(p)...);
    }

    T* alloc() {
        return (T*)PoolType::alloc(sizeof(Node));
    }
};


/**
 * Defines a pool allocator that allocates blocks of a pre-determined size.
 */
template<typename PoolType>
struct Pool_Block : PoolType {
    /**
     * Creates a pool with the provided block size.
     * If the block size is 0, you must call Create() before using the pool.
     * Initializing without a block size is not supported by all pool implementations.
     */
    template<typename... P>
    Pool_Block(Size blockSize, P... p) :
            PoolType{blockSize, p...},
            mBlockSize{(U32)blockSize} {}

    /**
     * Creates a pool with the provided size.
     * This should only be called if a block size of 0 was provided to the constructor.
     */
    void build(Size blockSize) {
        assert(mBlockSize == 0);
        mBlockSize = (U32)blockSize;
    }

    template<typename T, typename... P>
    T* create(P&&... p) {
        assert(mBlockSize != 0);
        return PoolType::template create<T>(mBlockSize, forward<P>(p)...);
    }

    void* alloc() {
        assert(mBlockSize != 0);
        return PoolType::alloc(mBlockSize);
    }

private:
    U32 mBlockSize;
};

template<typename T, typename Allocator = HeapAllocator>
using Pool = Pool_Object<T, BasePool_Alloc<Allocator>>;

template<typename Allocator = HeapAllocator>
using BlockPool = Pool_Block<BasePool_Alloc<Allocator>>;

/**
 * A pool that uses fixed-size blocks to provide O(1) object retrieval by index.
 * The pool uses paging to minimize the amount of memory wasted on unused objects,
 * while still allowing many slots.
 */
struct BaseSparsePool {
    /**
     * Initializes the sparse array.
     * @param elementSize The size in bytes of each element.
     * @param pageSize The size of each page, in elements, as a power of 2.
     */
    BaseSparsePool(Size elementSize, Size pageSizePower = 13) :
        elementSize(elementSize),
        pageSizePower(pageSizePower),
        mask(MakeMask())
    {}

    /**
     * Returns the object at the provided index.
     * The object must exist.
     */
    void* get(U32 index) const {
        auto data = pages[getPageIndex(index)];
        return data + getOffsetInPage(index);
    }

    /**
     * Allocates an object at the provided index.
     * This may allocate a new page if the page that corresponds to the index doesn't exist yet.
     */
    void* alloc(U32 index) {
        auto i = getPageIndex(index);
        auto p = getPage(i);
        return p + getOffsetInPage(index);
    }

private:

    Byte* getPage(U32 i) {
        if(pages.size() <= i) {
            pages.reserve(i + 1);
            for(Size j = pages.size(); j <= i; j++)
                pages << nullptr;
        }

        auto& ptr = pages[i];
        if(!ptr) ptr = (Byte*)HeapAllocator::alloc(getPageSize());

        return ptr;
    }

    U32 getPageIndex(U32 index) const {
        return index >> pageSizePower;
    }

    U32 getOffsetInPage(U32 index) const {
        return (index & mask) * elementSize;
    }

    U32 getPageSize() const {
        return elementSize * (1 << pageSizePower);
    }

    U32 MakeMask() const {
        U32 a = -1;
        return ~(a << pageSizePower);
    }

    Array<Byte*> pages;
    const U32 elementSize;
    const U32 pageSizePower;
    const U32 mask;
};

/**
 * Type-safe version of BaseSparsePool.
 */
template<class T>
struct SparsePool : BaseSparsePool {
    SparsePool(U32 pageSizePower) : BaseSparsePool(sizeof(T), pageSizePower) {}

    /**
     * Retrieves the object at the provided index.
     * The object must exist.
     */
    T& get(U32 index) const {
        return *(T*)BaseSparsePool::get(index);
    }

    /**
     * Allocates an object at the provided index without calling its constructor.
     */
    T& alloc(U32 index) {
        return *(T*)BaseSparsePool::alloc(index);
    }

    /**
     * Creates a new instance of T at the provided index.
     */
    template<class... P>
    T& create(U32 index, P&&... params) {
        auto& o = alloc(index);
        new (&o) T(forward<P>(params)...);
        return o;
    }

    /**
     * Destroys the object at the provided index.
     */
    void destroy(U32 index) {
        auto& o = get(index);
        o.~T();
    }
};

#endif // Tritium_Core_Mem_Pool_h
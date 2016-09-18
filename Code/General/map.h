//--------------------------------------------------------------
// Copyright © 2015 Rimmer Motzheim - All rights reserved
//--------------------------------------------------------------

#ifndef Tritium_Core_Map_h
#define Tritium_Core_Map_h

#include "types.h"
#include "maybe.h"
#include "map.h"
#include "array.h"
#include "pool.h"

namespace Tritium {
namespace Internal {

template<class Key, class T, class A, Size size>
struct MapData {
    struct Entry {
        Entry(const Key& key) : key(key) {}
        T* get() {return data;}
        T* data;
        Key key;
    };

    using Arr = Array<Entry, A>;

    MapData(Size reservedSize) : pool(reservedSize), entries((U32)reservedSize) {}

    Entry* create(Size index, const Key& key) {
        auto e = entries.insert(index, key).p;
        e->data = pool.alloc();
        return e;
    }

    void remove(Size index) {
        auto e = entries.begin().p + index;
        pool.destroy(e->data);
        entries.remove(index);
    }

    Pool<T, A> pool;
    Arr entries;
};

template<class Key, class T, class A>
struct InlineMapData {
    struct Entry {
        Entry(const Key& key) : key(key) {}
        T* get() {return &data;}
        Uninitialized<T> data;
        Key key;
    };

    using Arr = Array<Entry, A>;

    InlineMapData(Size reservedSize) : entries((U32)reservedSize) {}

    Entry* create(Size index, const Key& key) {
        return entries.insert(index, key).p;
    }

    void remove(Size index) {
        auto e = entries.begin().p + index;
        e->data->~T();
        entries.remove(index);
    }

    Arr entries;
};

template<class Key, class T, class A>
struct MapData<Key, T, A, 8> : InlineMapData<Key, T, A> {using InlineMapData<Key, T, A>::InlineMapData;};

template<class Key, class T, class A>
struct MapData<Key, T, A, 4> : InlineMapData<Key, T, A> {using InlineMapData<Key, T, A>::InlineMapData;};

template<class Key, class T, class A>
struct MapData<Key, T, A, 2> : InlineMapData<Key, T, A> {using InlineMapData<Key, T, A>::InlineMapData;};

template<class Key, class T, class A>
struct MapData<Key, T, A, 1> : InlineMapData<Key, T, A> {using InlineMapData<Key, T, A>::InlineMapData;};

} // namespace Internal

//---------------------------------------------------------------------------------------------------------

template<class T>
struct Compare {
    static Int compare(const T& a, const T& b) {
        if(a == b) return 0;
        else if(a < b) return -1;
        else return 1;
    }
};

template<class Key, class T, class Comparator = Compare<Key>, class Allocator = HeapAllocator>
struct Map : Allocator {
    Map() : map(0) {}

    Map(Size reservedSize) : map(reservedSize) {}

    Maybe<const T*> get(const Key& key) const {
        Size index;
        if(auto entry = search(key, index)) {
            return Just(entry->get());
        } else {
            return Nothing();
        }
    }

    Maybe<T*> get(const Key& key) {
        Size index;
        if(auto entry = search(key, index)) {
            return Just(entry->get());
        } else {
            return Nothing();
        }
    }

    bool add(const Key& key, const T& data, bool overwrite = true) {
        T* created;
        auto existed = add(key, created, overwrite);
        if(created)
            new (created) T(data);
        return existed;
    }

    bool add(const Key& key, T&& data, bool overwrite = true) {
        T* created;
        auto existed = add(key, created, overwrite);
        if(created)
            new (created) T(forward<T>(data));
        return existed;
    }

    /**
     * Adds an entry with the specified key to the map if it doesn't exist yet.
     * Otherwise, the existing item is returned.
     * @param key The key to add.
     * @param out Will be set to the existing or allocated item data.
     * @param overwrite If set, the destructor of any existing item is called.
     * @return True if an item already existed.
     */
    bool add(const Key& key, T*& outData, bool overwrite = true) {
        if(!size()) {
            auto entry = map.create(0, key);
            outData = entry->get();
            return false;
        }

        Size index;
        auto found = search(key, index);
        if(found) {
            if(overwrite) {
                found->get()->~T();
                outData = found->get();
            } else {
                outData = nullptr;
            }
            return true;
        } else {
            auto entry = map.create(index, key);
            outData = entry->get();
            return false;
        }
    }

    /**
     * Adds an entry with the specified key to the map if it doesn't exist yet.
     * Otherwise, the existing item is returned.
     * @param key The key to add.
     * @param out Will be set to the existing or allocated item data.
     * @return True if an item already existed.
     */
    bool addGet(const Key& key, T*& out) {
        return add(key, out, false);
    }

    /**
     * Removes the element with the provided key, if any exists.
     * @return True if an element was removed.
     */
    Bool remove(const Key& key) {
        Size index;
        if(search(key, index)) {
            map.remove(index);
            return true;
        } else {
            return false;
        }
    }

    /**
     * Removes the element with the provided key, if any exists.
     * @param fun This function is called on the element about to be removed, if any.
     * @return True if an element was removed.
     */
    template<class F>
    Bool remove(const Key& key, F&& fun) {
        Size index;
        if(auto entry = search(key, index)) {
            fun(*entry->data);
            map.remove(index);
            return true;
        } else {
            return false;
        }
    }

    void clear() {
        for(Int i = size(); i > 0; i--) {
            map.remove(size() - 1);
        }
    }

    Size size() const {
        return map.entries.size();
    }

    T& operator[] (const Key& key) {
        T* value;
        if(!addGet(key, value))
            new (value) T;
        return *value;
    }

private:
    using Data = Internal::MapData<Key, T, Allocator, sizeof(T)>;
    using Entry = typename Data::Entry;

    Entry* search(const Key& src, Size& insertPos) const {
        Int low = 0;
        Int count = (Int)size();
        Int high = count - 1;

        while(low <= high) {
            Int mid = (low + high) / 2;
            auto entry = map.entries.begin().p + mid;

            Int res = Comparator::compare(entry->key, src);
            if(res < 0) {
                low = mid + 1;
            } else if(res > 0) {
                high = mid - 1;
            } else {
                insertPos = (Size)mid;
                return const_cast<Entry*>(entry);
            }
        }

        insertPos = (Size)low;
        return nullptr;
    }

public:
    using I = typename Data::Arr::I;
    using CI = typename Data::Arr::CI;

    I begin() {return map.entries.begin();}
    I end() {return map.entries.end();}
    CI begin() const {return map.entries.begin();}
    CI end() const {return map.entries.end();}

private:
    Data map;
};

//----------------------------------------------------------------------------------------------------------

template<class K, class T, class A, class F>
void walk(F f, const Map<K, T, A>& map) {
    for(auto& i : map) {f(i.key, *i.data);}
}

template<class K, class T, class A, class F>
void modify(F f, Map<K, T, A>& map) {
    for(auto& i : map) {f(i.key, *i.data);}
}

template<class K, class T, class A, class U, class F>
auto fold(F f, U start, typename Map<K, T, A>::CI begin, typename Map<K, T, A>::CI end) {
    if(begin != end) {
        auto next = begin; ++next;
        return fold(f, f(*begin, start), next, end);
    }
    else return start;
}

template<class K, class T, class A, class U, class F>
auto fold(F f, U start, const Map<K, T, A>& map) {
    return fold(f, start, map.begin(), map.end());
}

} // namespace Tritium

#endif // Tritium_Core_Map_h
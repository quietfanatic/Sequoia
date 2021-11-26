#pragma once

#include "../util/assert.h"
#include "../util/types.h"

namespace model {

template <class T>
struct IDHandle {
    int64 id;

    explicit constexpr IDHandle (int64 id = 0) : id(id) { AA(id >= 0); }
    IDHandle (const T& data) : id(data.id) { AA(id); }
    IDHandle (const IDHandle& o) : id(o.id) { }
    constexpr operator int64 () const { return id; }

     // If you want to modify the data, you have to copy it first.
     //  The address of the return is not guaranteed to be stable, so don't
     //  keep it around anywhere.
    const T& operator * () const {
        AA(id);
        static int64 last_id = 0;
        static const T* last_data;
        if (last_id != id) {
            last_id = id;
            last_data = T::load(*this);
        }
        return *last_data;
    }
    const T* operator -> () const { return &**this; }
};

struct PageData;
using PageID = IDHandle<PageData>;
struct LinkData;
using LinkID = IDHandle<LinkData>;
struct ViewData;
using ViewID = IDHandle<ViewData>;

} // namespace model
namespace std {
    template <class T>
    struct hash<model::IDHandle<T>> {
        std::size_t operator () (model::IDHandle<T> v) const {
            return std::hash<int64>{}(v.id);
        }
    };
}

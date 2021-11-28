#pragma once

#include "../util/assert.h"
#include "../util/types.h"

namespace model {

template <class T>
struct ModelID {
    int64 id;

    explicit constexpr ModelID (int64 id = 0) : id(id) { AA(id >= 0); }
    ModelID (const ModelID& o) : id(o.id) { }
    explicit constexpr operator bool () const { return id; }
    constexpr operator int64 () const { return id; }
};

inline namespace page { struct PageData; }
using PageID = ModelID<PageData>;
inline namespace link { struct LinkData; }
using LinkID = ModelID<LinkData>;
inline namespace view { struct ViewData; }
using ViewID = ModelID<ViewData>;

} // namespace model
namespace std {
    template <class T>
    struct hash<model::ModelID<T>> {
        std::size_t operator () (model::ModelID<T> v) const {
            return std::hash<int64>{}(v.id);
        }
    };
}

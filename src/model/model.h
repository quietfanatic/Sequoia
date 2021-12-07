#pragma once

#include "../util/error.h"
#include "../util/types.h"

namespace model {

struct Model;
Model& new_model (Str db_path);
void delete_model (Model&);

 // A readable view of the model.  Doesn't currently do anything real.
 // Eventually this might support thread-safe read transactions.
struct Read {
    const Model& model;
    Read (const Model& m) : model(m) { }
};
 // Just a const Read&, but collapses one pointer indirection
struct ReadRef {
    const Model& model;
    ReadRef (const Read& r) : model(r.model) { }
     // For some reason the compiler is unable to stack the Read and ReadRef
     // implicit coercions together.  TODO: investigate why
    ReadRef (const Model& m) : model(m) { }
    const Model& operator* () const { return model; }
    const Model* operator-> () const { return &model; }
};

 // A writable view of the model, defined in write.h
 // Unlike Read, this does not implicitly cast from Model&.
struct Write;
struct WriteRef;

template <class T>
struct ModelID {
    int64 id;

    explicit constexpr ModelID (int64 id = 0) : id(id) { AA(id >= 0); }
    ModelID (const ModelID& o) : id(o.id) { }
    explicit constexpr operator bool () const { return id; }
    constexpr operator int64 () const { return id; }
};

struct PageData;
using PageID = ModelID<PageData>;
struct LinkData;
using LinkID = ModelID<LinkData>;
struct ViewData;
using ViewID = ModelID<ViewData>;

 // Loading operators.  I'll go ahead and declare them here so that you ge
 // better error messages if you forget to include page.h or similar.
 //
 // Probably ->* would make more sense, but it's annoying to type and unclear
 //  whether its an overload when you're reading it.  We'd totally use ->* if
 //  it had the same precedence as ->, so you could say
 //  model->*page->title...but it doesn't, so (model/page)->title it is.
 //  operator[] would also be great, but it can't be a non-member function,
 //  for no particularly good reason I can see.
const PageData* operator / (ReadRef, PageID);
const LinkData* operator / (ReadRef, LinkID);
const ViewData* operator / (ReadRef, ViewID);

} // namespace model

 // Allow ModelID in unordered_map and unordered_set
namespace std {
    template <class T>
    struct hash<model::ModelID<T>> {
        std::size_t operator () (model::ModelID<T> v) const {
            return std::hash<int64>{}(v.id);
        }
    };
}

#pragma once

 // This file provides non-thread-safe weak pointer support.

#include <type_traits>

#include "types.h"

struct WeakPointable {
    WeakPointable ();
    ~WeakPointable ();
  private:
    template <class T> friend struct WeakPtr;
    const size_t weak_id;
    static WeakPointable* get_strong_ptr (size_t id);
};

template <class T>
struct WeakPtr {
    static_assert(std::is_base_of_v<WeakPointable, T>);

    WeakPtr (T* ptr = nullptr) : id(
        ptr ? static_cast<WeakPointable*>(ptr)->weak_id : 0
    ) { }

    operator T* () const {
        if (!id) return nullptr;
        return static_cast<T*>(WeakPointable::get_strong_ptr(id));
    }
    explicit operator bool () const { return this->operator T*(); }
    T& operator * () const {
        WeakPointable* ptr = this->operator T*();
        AA(ptr);
        return *ptr;
    }
    T* operator -> () const { return this->operator T*(); }

  private:
    size_t id = 0;
};

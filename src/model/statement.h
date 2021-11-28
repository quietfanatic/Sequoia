#pragma once

#include <optional>
#include <type_traits>
#include <vector>

#include <sqlite3.h>

#include "../util/assert.h"
#include "../util/bifractor.h"
#include "model.h"

namespace model {

 // Took way too many tries to get this correct
template <class T, class Def = const std::remove_cvref_t<T>&>
std::optional<std::remove_cvref_t<T>> null_default (T&& v, Def def = Def{}) {
    if (v == def) return std::nullopt;
    else return {std::forward<T>(v)};
}

 // getting awkward
template <class T>
constexpr bool is_optional = false;
template <class T>
constexpr bool is_optional<std::optional<T>> = true;

struct Statement {
    sqlite3_stmt* handle;
    Statement () : handle(nullptr) { }
    Statement (sqlite3* db, Str sql, bool transient = false) {
        auto flags = transient ? 0 : SQLITE_PREPARE_PERSISTENT;
        AS(db, sqlite3_prepare_v3(db, sql.data(), int(sql.size()), flags, &handle, nullptr));
    }
    Statement (const Statement&) = delete;
    Statement& operator= (const Statement&) = delete;
    Statement (Statement&& st) : handle(st.handle) { st.handle = nullptr; }
    Statement& operator= (Statement&& st) {
        this->~Statement();
        new (this) Statement (std::move(st));
    }
    ~Statement () {
        sqlite3* db = sqlite3_db_handle(handle);
        if (handle) AS(db, sqlite3_finalize(handle));
    }
    operator sqlite3_stmt*& () { return handle; }
};

struct UseStatement {
    sqlite3_stmt* handle;
    int index = 1;
    bool done = false;

    UseStatement (sqlite3_stmt* h) : handle(h) { }

    sqlite3* db () { return sqlite3_db_handle(handle); }

    void param (char v) { AS(db(), sqlite3_bind_int(handle, index++, v)); }
    void param (signed char v) { AS(db(), sqlite3_bind_int(handle, index++, v)); }
    void param (unsigned char v) { AS(db(), sqlite3_bind_int(handle, index++, v)); }
    void param (short v) { AS(db(), sqlite3_bind_int(handle, index++, v)); }
    void param (unsigned short v) { AS(db(), sqlite3_bind_int(handle, index++, v)); }
    void param (int v) { AS(db(), sqlite3_bind_int(handle, index++, v)); }
    void param (unsigned int v) { AS(db(), sqlite3_bind_int(handle, index++, v)); }
    void param (long v) { AS(db(), sqlite3_bind_int64(handle, index++, v)); }
    void param (unsigned long v) { AS(db(), sqlite3_bind_int64(handle, index++, v)); }
    void param (long long v) { AS(db(), sqlite3_bind_int64(handle, index++, v)); }
    void param (unsigned long long v) { AS(db(), sqlite3_bind_int64(handle, index++, v)); }
    void param (float v) { AS(db(), sqlite3_bind_double(handle, index++, v)); }
    void param (double v) { AS(db(), sqlite3_bind_double(handle, index++, v)); }
    void param (const char* v) {
        AS(db(), sqlite3_bind_text(handle, index++, v, -1, SQLITE_TRANSIENT));
    }
    void param (Str v) {
         // STATIC might be better for most use cases
        AS(db(), sqlite3_bind_text(handle, index++, v.data(), int(v.size()), SQLITE_TRANSIENT));
    }
    void param (const Bifractor& v) {
        AS(db(), sqlite3_bind_blob(handle, index++, v.bytes(), int(v.size), SQLITE_TRANSIENT));
    }
    template <class T>
    void param (const std::optional<T>& v) {
        if (v) param(*v);
        else AS(db(), sqlite3_bind_null(handle, index++));
    }
    template <class T>
    void param (ModelID<T> v) {
        param(int64(v));
    }
    template <class T, std::enable_if_t<std::is_enum_v<T>, bool> = true>
    void param (T v) {
        param(std::underlying_type_t<T>{v});
    }

    template <class... Ts>
    void params (Ts&&... vs) {
        (param(vs), ...);
    }

    struct Column {
        const UseStatement* st;
        int index;

        operator bool () const { return sqlite3_column_int(st->handle, index); }
        operator int64 () const { return sqlite3_column_int64(st->handle, index); }
        operator double () const { return sqlite3_column_double(st->handle, index); }
        operator Str () {
            auto p = reinterpret_cast<const char*>(
                sqlite3_column_text(st->handle, index)
            );
            return p ? p : ""sv;
        }
        operator Bifractor () {
            const void* data = sqlite3_column_blob(st->handle, index);
            int size = sqlite3_column_bytes(st->handle, index);
            return Bifractor(data, size);
        }
        template <class T, std::enable_if_t<std::is_enum_v<T>, bool> = true>
        operator T () {
             // Who thought having {} warn about narrowing conversions was a
             //  good idea
            return T{std::underlying_type_t<T>(int64(*this))};
        }
        template <class T>
        operator std::optional<T> () {
            if (sqlite3_column_type(st->handle, index) == SQLITE_NULL) {
                return std::nullopt;
            }
            else {
                return read_column<T::value_type>(index);
            }
        }
        template <class T>
        operator ModelID<T> () {
            return ModelID<T>(int64(*this));
        }
    };
    Column operator [] (int index) { return Column{this, index}; }

    bool step () {
        AA(!done);
        auto result_code = sqlite3_step(handle);
        if (result_code != SQLITE_ROW && result_code != SQLITE_DONE) AS(db(), result_code);
        done = result_code == SQLITE_DONE;
        return !done;
    }

    bool optional () {
        if (step()) {
            AA(!step());
            return true;
        }
        else return false;
    }

    void single () {
        AA(step());
        AA(!step());
    }

    void run () {
        AA(!step());
    }

    template <class T>
    std::vector<T> collect () {
        std::vector<T> r;
        while (step()) {
            r.emplace_back((*this)[0]);
        }
        return r;
    }

    ~UseStatement () {
        AA(done);
        AS(db(), sqlite3_reset(handle));
        AS(db(), sqlite3_clear_bindings(handle));
    }
};

}

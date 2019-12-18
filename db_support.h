#pragma once

#include <tuple>
#include <utility>
#include <vector>

#include <sqlite3.h>

#include "assert.h"

namespace {

extern sqlite3* db;

template <class... Cols>
struct State {
template <class... Params>
struct Ment {
    sqlite3_stmt* handle;
    int result_code = 0;

    static constexpr std::index_sequence_for<Cols...> cols_indexes = {};
    static constexpr std::index_sequence_for<Params...> params_indexes = {};

    Ment (sqlite3_stmt* handle) : handle(handle) { }
    Ment (const char* sql, bool transient = false) {
        auto flags = transient ? 0 : SQLITE_PREPARE_PERSISTENT;
        AS(sqlite3_prepare_v3(db, sql, -1, flags, &handle, nullptr));
    }

    void bind_param (int index, float v) {
        AS(sqlite3_bind_double(handle, index, v));
    }
    void bind_param (int index, double v) {
        AS(sqlite3_bind_double(handle, index, v));
    }
    void bind_param (int index, const char* v) {
        AS(sqlite3_bind_text(handle, index, v, -1, SQLITE_TRANSIENT));
    }
    void bind_param (int index, const std::string& v) {
         // STATIC might be better for most use cases
        AS(sqlite3_bind_text(handle, index, v.c_str(), int(v.size()), SQLITE_TRANSIENT));
    }
    template <class T>
    void bind_param (int index, T&& v) {
        AS(sqlite3_bind_int64(handle, index, std::forward<T>(v)));
    }

    template <size_t... indexes>
    void bind_with_indexes (const Params&... params, std::index_sequence<indexes...>) {
        (bind_param(indexes+1, params), ...);
    }

    void bind (const Params&... params) {
        bind_with_indexes(params..., params_indexes);
    }

    void step () {
        result_code = sqlite3_step(handle);
        if (result_code != SQLITE_ROW && result_code != SQLITE_DONE) AS(1);
    }

    template <class T>
    T read_column (int index) {
        return T(sqlite3_column_int64(handle, index));
    }
    template <>
    const char* read_column<const char*> (int index) {
         // This probably isn't safe to use with run_*
        return reinterpret_cast<const char*>(sqlite3_column_text(handle, index));
    }
    template <>
    std::string read_column<std::string> (int index) {
        auto p = reinterpret_cast<const char*>(sqlite3_column_text(handle, index));
        return p ? p : "";
    }
    template <>
    float read_column<float> (int index) {
        return sqlite3_column_double(handle, index);
    }
    template <>
    double read_column<double> (int index) {
        return sqlite3_column_double(handle, index);
    }

    template <size_t... indexes>
    std::tuple<Cols...> read_with_indexes (std::index_sequence<indexes...>) {
        return std::make_tuple(read_column<Cols>(indexes)...);
    }

    std::tuple<Cols...> read () {
        return read_with_indexes(cols_indexes);
    }

    bool done () {
        return result_code == SQLITE_DONE;
    }

    void finish () {
        AS(sqlite3_reset(handle));
        AS(sqlite3_clear_bindings(handle));
        result_code = 0;
    }

    // This is probably not efficient for large data sets
    std::vector<std::tuple<Cols...>> run (const Params&... params) {
        std::vector<std::tuple<Cols...>> r;
        bind(params...);
        step();
        A(sqlite3_column_count(handle) == sizeof...(Cols));
        while (!done()) {
            r.emplace_back(read());
            step();
        }
        finish();
        return r;
    }

    void run_void (const Params&... params) {
        bind(params...);
        step();
        A(done());
        finish();
    }

    std::tuple<Cols...> run_single (const Params&... params) {
        bind(params...);
        step();
        A(sqlite3_column_count(handle) == sizeof...(Cols));
        std::tuple<Cols...> r = read();
        step();
        A(done());
        finish();
        return r;
    }
};
};

}

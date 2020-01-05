#pragma once

#include <tuple>
#include <utility>
#include <vector>

#include <sqlite3.h>

#include "assert.h"
#include "bifractor.h"

 // Kinda cheating but whatever
extern sqlite3* db;

struct Statement {
    sqlite3_stmt* handle;
    int result_code = 0;

    Statement (sqlite3_stmt* handle) : handle(handle) { }
    Statement (const char* sql, bool transient = false) {
        auto flags = transient ? 0 : SQLITE_PREPARE_PERSISTENT;
        AS(sqlite3_prepare_v3(db, sql, -1, flags, &handle, nullptr));
    }

    void bind_param (int index, char v) { AS(sqlite3_bind_int(handle, index, v)); }
    void bind_param (int index, signed char v) { AS(sqlite3_bind_int(handle, index, v)); }
    void bind_param (int index, unsigned char v) { AS(sqlite3_bind_int(handle, index, v)); }
    void bind_param (int index, short v) { AS(sqlite3_bind_int(handle, index, v)); }
    void bind_param (int index, unsigned short v) { AS(sqlite3_bind_int(handle, index, v)); }
    void bind_param (int index, int v) { AS(sqlite3_bind_int(handle, index, v)); }
    void bind_param (int index, unsigned int v) { AS(sqlite3_bind_int(handle, index, v)); }
    void bind_param (int index, long v) { AS(sqlite3_bind_int64(handle, index, v)); }
    void bind_param (int index, unsigned long v) { AS(sqlite3_bind_int64(handle, index, v)); }
    void bind_param (int index, long long v) { AS(sqlite3_bind_int64(handle, index, v)); }
    void bind_param (int index, unsigned long long v) { AS(sqlite3_bind_int64(handle, index, v)); }
    void bind_param (int index, float v) { AS(sqlite3_bind_double(handle, index, v)); }
    void bind_param (int index, double v) { AS(sqlite3_bind_double(handle, index, v)); }
    void bind_param (int index, const char* v) {
        AS(sqlite3_bind_text(handle, index, v, -1, SQLITE_TRANSIENT));
    }
    void bind_param (int index, const std::string& v) {
         // STATIC might be better for most use cases
        AS(sqlite3_bind_text(handle, index, v.c_str(), int(v.size()), SQLITE_TRANSIENT));
    }
    void bind_param (int index, const Bifractor& v) {
        AS(sqlite3_bind_blob(handle, index, v.bytes(), int(v.size), SQLITE_TRANSIENT));
    }

    void step () {
        A(result_code != SQLITE_DONE);
        result_code = sqlite3_step(handle);
        if (result_code != SQLITE_ROW && result_code != SQLITE_DONE) AS(1);
    }

    // Assume int for all other types
    template <class T>
    T read_column (int index) {
        return T(sqlite3_column_int64(handle, index));
    }
    template <>
    float read_column<float> (int index) {
        return float(sqlite3_column_double(handle, index));
    }
    template <>
    double read_column<double> (int index) {
        return sqlite3_column_double(handle, index);
    }
    template <>
    std::string read_column<std::string> (int index) {
        auto p = reinterpret_cast<const char*>(sqlite3_column_text(handle, index));
        return p ? p : "";
    }
    template <>
    Bifractor read_column<Bifractor> (int index) {
        const void* data = sqlite3_column_blob(handle, index);
        int size = sqlite3_column_bytes(handle, index);
        return Bifractor(data, size);
    }

    bool done () {
        return result_code == SQLITE_DONE;
    }

    void reset () {
        AS(sqlite3_reset(handle));
        AS(sqlite3_clear_bindings(handle));
        result_code = 0;
    }

};

template <class... Ts>
struct ResultType {
    using type = std::tuple<Ts...>;
};

template <class T>
struct ResultType<T> {
    using type = T;
};

template <class... Cols>
struct State {
template <class... Params>
struct Ment : Statement {
    using Result = typename ResultType<Cols...>::type;

    static constexpr std::index_sequence_for<Cols...> cols_indexes = {};
    static constexpr std::index_sequence_for<Params...> params_indexes = {};

    using Statement::Statement; // inherit constructor

    template <size_t... indexes>
    void bind_with_indexes (const Params&... params, std::index_sequence<indexes...>) {
        (bind_param(indexes+1, params), ...);
    }

    void bind (const Params&... params) {
        bind_with_indexes(params..., params_indexes);
    }


    template <size_t... indexes>
    Result read_with_indexes (std::index_sequence<indexes...>) {
        return Result{read_column<Cols>(indexes)...};
    }

    Result read () {
        return read_with_indexes(cols_indexes);
    }

    // This is probably not efficient for large rows, due to resizing the vector
    std::vector<Result> run (const Params&... params) {
        std::vector<Result> r;
        bind(params...);
        step();
        A(sqlite3_column_count(handle) == sizeof...(Cols));
        while (!done()) {
            r.emplace_back(read());
            step();
        }
        reset();
        return r;
    }

    void run_void (const Params&... params) {
        bind(params...);
        step();
        A(done());
        reset();
    }

    Result run_single (const Params&... params) {
        bind(params...);
        step();
        A(sqlite3_column_count(handle) == sizeof...(Cols));
        Result r = read();
        step();
        A(done());
        reset();
        return r;
    }

    Result run_or (const Params&... params, const Result& def) {
        bind(params...);
        step();
        if (done()) {
            reset();
            return def;
        }
        A(sqlite3_column_count(handle) == sizeof...(Cols));
        Result r = read();
        step();
        A(done());
        reset();
        return r;
    }
};
};

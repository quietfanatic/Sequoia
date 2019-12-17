#include "data.h"

#include <chrono>
#include <filesystem>
#include <sqlite3.h>

#include "assert.h"
#include "hash.h"
#include "logging.h"
#include "util.h"

using namespace std;
using namespace std::chrono;

///// Various database-related helpers

namespace {

std::vector<Observer*> all_observers;
std::vector<int64> updated_tabs;

void tab_updated (int64 id) {
    for (auto t : updated_tabs) {
        if (t == id) return;
    }
    updated_tabs.push_back(id);
}

//
//static void bind_param (sqlite3_stmt* stmt, int index, int64 v) {
//    AS(sqlite3_bind_int64(stmt, index, v));
//}
//static void bind_param (sqlite3_stmt* stmt, int index, double v) {
//    AS(sqlite3_bind_double(stmt, index, v));
//}
//static void bind_param (sqlite3_stmt* stmt, int index, const string& v) {
//    AS(sqlite3_bind_text(stmt, index, v.c_str(), v.size(), SQLITE_STATIC));
//}
//
//template <int i, class... Args>
//void bind_params_i (sqlite3_stmt* stmt, Args&&... args);
//template <int i>
//void bind_params_i<i> (sqlite3_stmt* stmt) { }
//template <int i, class Arg, class... Args>
//void bind_params_i<i, Arg, Args...> (sqlite3_stmt* stmt, Arg&& arg, Args&&... args) {
//    bind_param(stmt, i, forward<Arg>(arg));
//    bind_params_i(stmt, i+1, args...);
//}
//
//template <class... Args>
//void bind_params (sqlite3_stmt* stmt, Args&&... args) {
//    bind_params_i(stmt, 1, args...);
//}

double now () {
    return duration<double>(system_clock::now().time_since_epoch()).count();
}

///// Setup

sqlite3* db = nullptr;

void init_db () {
    if (db) return;
    LOG("init_db");
    string db_file = exe_relative("db.sqlite");
    bool exists = filesystem::exists(db_file) && filesystem::file_size(db_file) > 0;
    AS(sqlite3_open(db_file.c_str(), &db));
    if (!exists) {
        string schema = slurp(exe_relative("res/schema.sql"));
        LOG("Creating database...");
        AS(sqlite3_exec(db, schema.c_str(), nullptr, nullptr, nullptr));
        LOG("Created database.");
    }
}

sqlite3_stmt* make_statement(const char* sql) {
    init_db();
    sqlite3_stmt* stmt;
    sqlite3_prepare_v3(db, sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt, nullptr);
    return stmt;
}

} // namespace

///// API

void begin_transaction () { }

void commit_transaction () {
    for (auto o : all_observers) {
        o->Observer_after_commit(updated_tabs);
    }
    updated_tabs.clear();
}

void rollback_transaction () { }

int64 create_webpage_tab (int64 parent, const string& url, const string& title) {
    static sqlite3_stmt* create = make_statement(
R"(
INSERT INTO tabs (parent, tab_type, url_hash, url, title, created_at)
VALUES (?1, ?2, ?3, ?4, ?5, ?6)
)"
    );

    AS(sqlite3_bind_int64(create, 1, parent));
    AS(sqlite3_bind_int(create, 2, WEBPAGE));
    AS(sqlite3_bind_int64(create, 3, x31_hash(url.c_str())));
    AS(sqlite3_bind_text(create, 4, url.c_str(), int(url.size()), SQLITE_TRANSIENT));
    AS(sqlite3_bind_text(create, 5, title.c_str(), int(title.size()), SQLITE_TRANSIENT));
    AS(sqlite3_bind_double(create, 6, now()));

    int code = sqlite3_step(create);
    if (code != SQLITE_DONE) AS(1);
    AS(sqlite3_clear_bindings(create));
    int64 id = sqlite3_last_insert_rowid(db);
    tab_updated(id);
    commit_transaction();
    return id;
}

TabData get_tab_data (int64 id) {
    static sqlite3_stmt* get = make_statement(
R"(
SELECT parent, next, prev, child_count, tab_type, url, title, created_at, trashed_at, loaded_at FROM tabs WHERE id = ?
)"
    );

    AS(sqlite3_bind_int64(get, 1, id));

    int code = sqlite3_step(get);
    A(code != SQLITE_DONE);
    if (code != SQLITE_ROW) AS(1);

    auto null_is_empty = [](const unsigned char* s){
        return s ? reinterpret_cast<const char*>(s) : "";
    };
    TabData r {
        sqlite3_column_int64(get, 0),
        sqlite3_column_int64(get, 1),
        sqlite3_column_int64(get, 2),
        sqlite3_column_int64(get, 3),
        uint8(sqlite3_column_int(get, 4)),
        null_is_empty(sqlite3_column_text(get, 5)),
        null_is_empty(sqlite3_column_text(get, 6)),
        sqlite3_column_double(get, 7),
        sqlite3_column_double(get, 8),
        sqlite3_column_double(get, 9)
    };
    code = sqlite3_step(get);
    if (code != SQLITE_DONE) AS(1);
    AS(sqlite3_clear_bindings(get));
    return r;
}

string get_tab_url (int64 id) {
    static sqlite3_stmt* get = make_statement(
R"(
SELECT url FROM tabs WHERE id = ?
)"
    );

    AS(sqlite3_bind_int64(get, 1, id));

    int code = sqlite3_step(get);
    A(code != SQLITE_DONE);
    if (code != SQLITE_ROW) AS(1);

    string url = reinterpret_cast<const char*>(sqlite3_column_text(get, 0));
    code = sqlite3_step(get);
    if (code != SQLITE_DONE) AS(1);
    AS(sqlite3_clear_bindings(get));
    return url;
}

void set_tab_url (int64 id, const string& url) {
    static sqlite3_stmt* set = make_statement(
R"(
UPDATE tabs SET url_hash = ?, url = ? WHERE id = ?
)"
    );

    AS(sqlite3_bind_int64(set, 1, x31_hash(url.c_str())));
    AS(sqlite3_bind_text(set, 2, url.c_str(), int(url.size()), SQLITE_TRANSIENT));
    AS(sqlite3_bind_int64(set, 3, id));

    int code = sqlite3_step(set);
    if (code != SQLITE_DONE) AS(1);
    AS(sqlite3_clear_bindings(set));
    tab_updated(id);
    commit_transaction();
}

void set_tab_title (int64 id, const string& title) {
    static sqlite3_stmt* set = make_statement(
R"(
UPDATE tabs SET title = ? WHERE id = ?
)"
    );

    AS(sqlite3_bind_text(set, 1, title.c_str(), int(title.size()), SQLITE_TRANSIENT));
    AS(sqlite3_bind_int64(set, 2, id));

    int code = sqlite3_step(set);
    if (code != SQLITE_DONE) AS(1);
    AS(sqlite3_clear_bindings(set));
    tab_updated(id);
    commit_transaction();
}

Observer::Observer () { all_observers.emplace_back(this); }
Observer::~Observer () {
    for (auto iter = all_observers.begin(); iter != all_observers.end(); iter++) {
        if (*iter == this) {
            all_observers.erase(iter);
            break;
        }
    }
}

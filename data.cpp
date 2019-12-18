#include "data.h"

#include <chrono>
#include <filesystem>
#include <sqlite3.h>

#include "assert.h"
#include "db_support.h"
#include "hash.h"
#include "logging.h"
#include "util.h"

using namespace std;
using namespace std::chrono;

///// Misc

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

static double now () {
    return duration<double>(system_clock::now().time_since_epoch()).count();
}

///// Transactions

static std::vector<Observer*> all_observers;
static std::vector<int64> updated_tabs;

static void tab_updated (int64 id) {
    for (auto t : updated_tabs) {
        if (t == id) return;
    }
    updated_tabs.push_back(id);
}

static size_t transaction_depth = 0;

Transaction::Transaction () {
    A(!uncaught_exceptions());
    init_db();
    if (!transaction_depth) {
        static State<>::Ment<> begin {"BEGIN"};
        begin.run_void();
    }
    transaction_depth += 1;
}
Transaction::~Transaction () {
    transaction_depth -= 1;
    if (!transaction_depth) {
        if (uncaught_exceptions()) {
            static State<>::Ment<> rollback {"ROLLBACK"};
            rollback.run_void();
        }
        else {
            static State<>::Ment<> commit {"COMMIT"};
            commit.run_void();
            for (auto o : all_observers) {
                o->Observer_after_commit(updated_tabs);
            }
        }
        updated_tabs.clear();
    }
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

///// API

int64 create_webpage_tab (int64 parent, const string& url, const string& title) {
    Transaction tr;
    LOG("create_webpage_tab", parent, url, title);
    static State<>::Ment<int64, uint8, uint64, string, string, double> create {R"(
INSERT INTO tabs (parent, tab_type, url_hash, url, title, created_at)
VALUES (?1, ?2, ?3, ?4, ?5, ?6)
    )"};

    create.run_void(parent, uint8(WEBPAGE), x31_hash(url.c_str()), url, title, now());
    int64 id = sqlite3_last_insert_rowid(db);
    tab_updated(id);
    return id;
}

TabData get_tab_data (int64 id) {
    init_db();
    LOG("get_tab_data", id);
    static State<int64, int64, int64, uint, uint8, string, string, double, double, double>
        ::Ment<int64> get {R"(
SELECT parent, next, prev, child_count, tab_type, url, title, created_at, trashed_at, loaded_at FROM tabs WHERE id = ?
    )"};
    return make_from_tuple<TabData>(get.run_single(id));
}

string get_tab_url (int64 id) {
    init_db();
    LOG("get_tab_url", id);
    static State<string>::Ment<int64> get {R"(
SELECT url FROM tabs WHERE id = ?
    )"};
    return std::get<0>(get.run_single(id));
}

void set_tab_url (int64 id, const string& url) {
    Transaction tr;
    LOG("set_tab_url", id, url);
    static State<>::Ment<uint64, string, int64> set {R"(
UPDATE tabs SET url_hash = ?, url = ? WHERE id = ?
    )"};
    set.run_void(x31_hash(url), url, id);
    tab_updated(id);
}

void set_tab_title (int64 id, const string& title) {
    Transaction tr;
    LOG("set_tab_title", id, title);
    static State<>::Ment<string, int64> set {R"(
UPDATE tabs SET title = ? WHERE id = ?
    )"};
    set.run_void(title, id);
    tab_updated(id);
}

void trash_tab (int64 id) {
    Transaction tr;
    LOG("trash_tab", id);
    static State<>::Ment<double, int64> trash {R"(
UPDATE tabs SET trashed_at = ? WHERE id = ?
    )"};
    trash.run(now(), id);
    tab_updated(id);
}

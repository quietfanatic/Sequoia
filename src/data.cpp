#include "data.h"

#include <chrono>
#include <filesystem>
#include <sqlite3.h>

#include "assert.h"
#include "db_support.h"
#include "hash.h"
#include "logging.h"
#include "settings.h"
#include "util.h"

using namespace std;
using namespace std::chrono;

///// Misc

sqlite3* db = nullptr;

void init_db () {
    if (db) return;
    string db_file = profile_folder + "/sequoia-state.sqlite";
    LOG("init_db", db_file);
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
static std::vector<int64> updated_windows;

static void tab_updated (int64 id) {
    A(id > 0);
    for (auto t : updated_tabs) {
        if (t == id) return;
    }
    updated_tabs.push_back(id);
}
static void window_updated (int64 id) {
    A(id > 0);
    for (auto w : updated_windows) {
        if (w == id) return;
    }
    updated_windows.push_back(id);
}

void update_observers () {
     // All this fuddling is to make sure updates are sequenced,
     // so Observer_after_commit doesn't have to be reentrant.
    static bool updating = false;
    static bool again = false;
    if (updating) {
        again = true;
        return;
    }
    updating = true;
    auto tabs = move(updated_tabs);
    auto windows = move(updated_windows);
    for (auto o : all_observers) {
        o->Observer_after_commit(tabs, windows);
    }
    updating = false;
    if (again) {
        again = false;
        update_observers();
    }
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
            updated_tabs.clear();
            updated_windows.clear();
        }
        else {
            static State<>::Ment<> commit {"COMMIT"};
            commit.run_void();
            update_observers();
        }
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

///// TAB HELPER STATMENTS

void change_child_count (int64 parent, int64 diff) {
    if (parent != 0) {
        static State<>::Ment<int64, int64> update_child_count {R"(
UPDATE tabs SET child_count = child_count + ? WHERE id = ?
        )"};
        update_child_count.run_void(diff, parent);
        tab_updated(parent);
    }
}

std::tuple<int64, int64, int64> get_links (int64 id) {
    static State<int64, int64, int64>::Ment<int64> get {R"(
SELECT parent, prev, next FROM tabs WHERE id = ?
    )"};
    return get.run_single(id);
}

void set_prev (int64 id, int64 prev) {
    static State<>::Ment<int64, int64> set {R"(
UPDATE tabs SET prev = ? WHERE id = ?
    )"};
    set.run_void(prev, id);
    tab_updated(id);
}

void set_next (int64 id, int64 next) {
    static State<>::Ment<int64, int64> set {R"(
UPDATE tabs SET next = ? WHERE id = ?
    )"};
    set.run_void(next, id);
    tab_updated(id);
}

void set_parent (int64 id, int64 parent) {
    static State<>::Ment<int64, int64> set {R"(
UPDATE tabs SET parent = ? WHERE id = ?
    )"};
    set.run_void(parent, id);
    tab_updated(id);
    change_child_count(parent, +1);
}

void place_tab (int64 id, int64 reference, TabRelation rel) {
    switch (rel) {
    case TabRelation::BEFORE: {
        int64 parent, prev;
        tie(parent, prev, ignore) = get_links(reference);
        set_parent(id, parent);
        set_next(prev, id);
        set_prev(id, prev);
        set_next(id, reference);
        set_prev(reference, id);
        break;
    }
    case TabRelation::AFTER: {
        int64 parent, next;
        tie(parent, ignore, next) = get_links(reference);
        set_parent(id, parent);
        set_next(reference, id);
        set_prev(id, reference);
        set_next(id, next);
        set_prev(next, id);
        break;
    }
    case TabRelation::FIRST_CHILD: {
        static State<int64>::Ment<int64> find_first {R"(
SELECT id FROM tabs WHERE closed_at IS NULL AND parent = ? AND prev = 0
        )"};
        vector<int64> maybe_first = find_first.run(reference);
        int64 next = maybe_first.empty() ? 0 : maybe_first[0];
        if (next) {
            set_prev(next, id);
            set_next(id, next);
        }
        set_parent(id, reference);
        break;
    }
    case TabRelation::LAST_CHILD: {
        static State<int64>::Ment<int64> find_last {R"(
SELECT id FROM tabs WHERE closed_at IS NULL AND parent = ? AND next = 0
        )"};
        vector<int64> maybe_last = find_last.run(reference);
        int64 prev = maybe_last.empty() ? 0 : maybe_last[0];

        if (prev) {
            set_next(prev, id);
            set_prev(id, prev);
        }
        set_parent(id, reference);
        break;
    }
    }
}

///// TABS

int64 create_webpage_tab (int64 reference, TabRelation rel, const string& url, const string& title) {
    Transaction tr;
    LOG("create_webpage_tab", reference, uint(rel), url, title);

    static State<>::Ment<uint8, uint64, string, string, double> create {R"(
INSERT INTO tabs (tab_type, url_hash, url, title, created_at)
VALUES (?, ?, ?, ?, ?)
    )"};
    create.run_void(uint8(WEBPAGE), x31_hash(url.c_str()), url, title, now());
    int64 id = sqlite3_last_insert_rowid(db);

    place_tab(id, reference, rel);


    return id;
}

TabData get_tab_data (int64 id) {
    init_db();
    LOG("get_tab_data", id);
    static State<int64, int64, int64, uint, uint8, string, string, double, double, double>
        ::Ment<int64> get {R"(
SELECT parent, prev, next, child_count, tab_type, url, title, created_at, visited_at, closed_at
FROM tabs WHERE id = ?
    )"};
    return make_from_tuple<TabData>(get.run_single(id));
}

std::vector<int64> get_all_children (int64 parent) {
    init_db();
    LOG("get_all_children", parent);
    static State<int64>::Ment<int64> get {R"(
SELECT id FROM tabs WHERE parent = ? AND closed_at IS NULL
    )"};
    return get.run(parent);
}

string get_tab_url (int64 id) {
    init_db();
    LOG("get_tab_url", id);
    static State<string>::Ment<int64> get {R"(
SELECT url FROM tabs WHERE id = ?
    )"};
    return get.run_single(id);
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

void close_tab (int64 id) {
    Transaction tr;
    LOG("close_tab", id);

    static State<int64, int64, int64, double>::Ment<int64> get_stuff {R"(
SELECT parent, prev, next, closed_at FROM tabs WHERE id = ?
    )"};
    int64 parent, prev, next;
    double closed_at;
    tie(parent, prev, next, closed_at) = get_stuff.run_single(id);

    if (closed_at) return;  // Already closed

    static State<>::Ment<double, int64> close {R"(
UPDATE tabs SET closed_at = ? WHERE id = ?
    )"};
    close.run_void(now(), id);
    tab_updated(id);

    if (prev) {
        static State<>::Ment<int64, int64> relink_prev {R"(
UPDATE tabs SET next = ? WHERE id = ?
        )"};
        relink_prev.run_void(next, prev);
        tab_updated(prev);
    }

    if (next) {
        static State<>::Ment<int64, int64> relink_next {R"(
UPDATE tabs SET prev = ? WHERE id = ?
        )"};
        relink_next.run_void(prev, next);
        tab_updated(next);
    }

    change_child_count(parent, -1);
}

void fix_counts () {
    LOG("fix_counts");
    Transaction tr;
    State<>::Ment<> fix_child_counts {R"(
WITH RECURSIVE ancestors (child, ancestor) AS (
    SELECT id, parent FROM tabs
    UNION ALL
    SELECT id, ancestor FROM tabs, ancestors
        WHERE parent = child
) UPDATE tabs SET child_count = (
    SELECT count(*) from ancestors where ancestor = id
)
    )", true};
    fix_child_counts.run_void();
}

///// WINDOWS

int64 create_window (int64 focused_tab) {
    Transaction tr;
    LOG("create_window", focused_tab);
    static State<>::Ment<int64, double> create {R"(
INSERT INTO windows (focused_tab, created_at) VALUES (?, ?)
    )"};
    create.run_void(focused_tab, now());
    int64 id = sqlite3_last_insert_rowid(db);
    window_updated(id);
    return id;
}

vector<WindowData> get_all_unclosed_windows () {
    init_db();
    LOG("get_all_unclosed_windows");
    static State<int64, int64, double, double>::Ment<> get {R"(
SELECT id, focused_tab, created_at, closed_at FROM windows
WHERE closed_at IS NULL
    )"};
     // Should be a way to avoid this copy but it's not very important
    auto results = get.run();
    vector<WindowData> r;
    r.reserve(results.size());
    for (auto& res : results) {
        r.push_back(make_from_tuple<WindowData>(res));
    }
    return r;
}

void set_window_focused_tab (int64 window, int64 tab) {
    Transaction tr;
    LOG("set_window_focused_tab", window, tab);
    static State<>::Ment<int64, int64> set {R"(
UPDATE windows SET focused_tab = ? WHERE id = ?
    )"};
    set.run_void(tab, window);
    window_updated(window);
}

int64 get_window_focused_tab (int64 window) {
    init_db();
    LOG("get_window_focused_tab", window);
    static State<int64>::Ment<int64> get {R"(
SELECT focused_tab FROM windows WHERE id = ?
    )"};
    return get.run_single(window);
}

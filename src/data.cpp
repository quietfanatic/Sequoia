#include "data.h"

#include <chrono>
#include <filesystem>
#include <map>
#include <sqlite3.h>

#include "settings.h"
#include "util/assert.h"
#include "util/db_support.h"
#include "util/files.h"
#include "util/hash.h"
#include "util/logging.h"

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
        Transaction tr;
        string schema = slurp(exe_relative("res/schema.sql"));
        LOG("Creating database...");
        AS(sqlite3_exec(db, schema.c_str(), nullptr, nullptr, nullptr));
        LOG("Created database.");
    }
}

static double now () {
    return duration<double>(system_clock::now().time_since_epoch()).count();
}

///// Cache

static std::map<int64, TabData> tabs_by_id;

TabData* cached_tab_data (int64 id) {
    auto iter = tabs_by_id.find(id);
    if (iter != tabs_by_id.end()) {
        return &iter->second;
    }
    return nullptr;
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
            tabs_by_id.clear();
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

///// TAB HELPER STATEMENTS

TabData* get_tab_data (int64 id) {
    if (auto data = cached_tab_data(id)) {
        return data;
    }

    static State<int64, int64, int64, int64, uint8, string, string, double, double, double>
        ::Ment<int64> get {R"(
SELECT parent, prev, next, child_count, tab_type, url, title, created_at, visited_at, closed_at
FROM tabs WHERE id = ?
    )"};

    auto res = tabs_by_id.emplace(id, make_from_tuple<TabData>(get.run_single(id)));
    return &res.first->second;
}

void change_child_count (int64 parent, int64 diff) {
    while (parent > 0) {
        if (auto data = cached_tab_data(parent)) {
            data->child_count += diff;
        }
        static State<>::Ment<int64, int64> update_child_count {R"(
UPDATE tabs SET child_count = child_count + ? WHERE id = ?
        )"};
        update_child_count.run_void(diff, parent);
        tab_updated(parent);
        parent = get_tab_data(parent)->parent;
    }
}

void set_prev (int64 id, int64 prev) {
    if (auto data = cached_tab_data(id)) {
        data->prev = prev;
    }
    static State<>::Ment<int64, int64> set {R"(
UPDATE tabs SET prev = ? WHERE id = ?
    )"};
    set.run_void(prev, id);
    tab_updated(id);
}

void set_next (int64 id, int64 next) {
    if (auto data = cached_tab_data(id)) {
        data->next = next;
    }
    static State<>::Ment<int64, int64> set {R"(
UPDATE tabs SET next = ? WHERE id = ?
    )"};
    set.run_void(next, id);
    tab_updated(id);
}

void set_parent (int64 id, int64 parent) {
    if (auto data = cached_tab_data(id)) {
        data->parent = parent;
    }
    static State<>::Ment<int64, int64> set {R"(
UPDATE tabs SET parent = ? WHERE id = ?
    )"};
    set.run_void(parent, id);
    tab_updated(id);
    change_child_count(parent, 1 + get_tab_data(id)->child_count);
}

void remove_tab (int64 id) {
    auto data = get_tab_data(id);
    if (data->prev) set_next(data->prev, data->next);
    if (data->next) set_prev(data->next, data->prev);
    change_child_count(data->parent, -1 - data->child_count);
    set_parent(id, -1);
}

void place_tab (int64 id, int64 reference, TabRelation rel) {
    switch (rel) {
    case TabRelation::BEFORE: {
        auto data = get_tab_data(reference);
        int64 prev = data->prev;
        set_parent(id, data->parent);
        if (prev) set_next(prev, id);
        set_prev(id, prev);
        set_next(id, reference);
        set_prev(reference, id);
        break;
    }
    case TabRelation::AFTER: {
        auto data = get_tab_data(reference);
        int64 next = data->next;
        set_parent(id, data->parent);
        set_next(reference, id);
        set_prev(id, reference);
        set_next(id, next);
        if (next) set_prev(next, id);
        break;
    }
    case TabRelation::FIRST_CHILD: {
        static State<int64>::Ment<int64> find_first {R"(
SELECT id FROM tabs WHERE closed_at IS NULL AND parent = ? AND prev = 0
        )"};
        vector<int64> maybe_first = find_first.run(reference);
        int64 next = maybe_first.empty() ? 0 : maybe_first[0];
        set_prev(id, 0);
        set_next(id, next);
        if (next) set_prev(next, id);
        set_parent(id, reference);
        break;
    }
    case TabRelation::LAST_CHILD: {
        static State<int64>::Ment<int64> find_last {R"(
SELECT id FROM tabs WHERE closed_at IS NULL AND parent = ? AND next = 0
        )"};
        vector<int64> maybe_last = find_last.run(reference);
        int64 prev = maybe_last.empty() ? 0 : maybe_last[0];

        if (prev) set_next(prev, id);
        set_prev(id, prev);
        set_next(id, 0);
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


std::vector<int64> get_all_children (int64 parent) {
    LOG("get_all_children", parent);
    static State<int64>::Ment<int64> get {R"(
SELECT id FROM tabs WHERE parent = ? AND closed_at IS NULL
    )"};
    return get.run(parent);
}

string get_tab_url (int64 id) {
    return get_tab_data(id)->url;
}

void set_tab_url (int64 id, const string& url) {
    LOG("set_tab_url", id, url);
    Transaction tr;

    if (auto data = cached_tab_data(id)) {
        data->url = url;
    }
    static State<>::Ment<uint64, string, int64> set {R"(
UPDATE tabs SET url_hash = ?, url = ? WHERE id = ?
    )"};
    set.run_void(x31_hash(url), url, id);
    tab_updated(id);
}

void set_tab_title (int64 id, const string& title) {
    LOG("set_tab_title", id, title);
    Transaction tr;

    if (auto data = cached_tab_data(id)) {
        data->title = title;
    }
    static State<>::Ment<string, int64> set {R"(
UPDATE tabs SET title = ? WHERE id = ?
    )"};
    set.run_void(title, id);
    tab_updated(id);
}

void close_tab (int64 id) {
    LOG("close_tab", id);
    Transaction tr;

    auto data = get_tab_data(id);

    if (data->closed_at) return;  // Already closed

    int64 old_parent = data->parent;

    data->closed_at = now();
    static State<>::Ment<double, int64> close {R"(
UPDATE tabs SET closed_at = ? WHERE id = ?
    )"};
    close.run_void(data->closed_at, id);

    remove_tab(id);
    set_parent(id, old_parent);
}

void move_tab (int64 id, int64 reference, TabRelation rel) {
    if (reference == id) return;
    LOG("move_tab", id, reference, uint(rel));
    Transaction tr;

    remove_tab(id);
    place_tab(id, reference, rel);
}

void fix_problems () {
    LOG("fix_problems");
    Transaction tr;

    tabs_by_id.clear();
    State<>::Ment<> fix_child_counts {R"(
WITH RECURSIVE ancestors (child, ancestor) AS (
    SELECT id, parent FROM tabs WHERE closed_at IS NULL
    UNION ALL
    SELECT id, ancestor FROM tabs, ancestors
        WHERE closed_at IS NULL AND parent = child
)
UPDATE tabs SET child_count = (
    SELECT count(*) from ancestors where ancestor = id
)
    )", true};
    fix_child_counts.run_void();

    State<>::Ment<> fix_orphans {R"(
UPDATE tabs AS a SET parent = CASE
    WHEN prev > 0 AND (SELECT parent FROM tabs b WHERE b.id = a.prev) > 0
        THEN (SELECT parent FROM tabs b WHERE b.id = a.prev)
    WHEN next > 0 AND (SELECT parent FROM tabs b WHERE b.id = a.next) > 0
        THEN (SELECT parent FROM tabs b WHERE b.id = a.next)
    ELSE 0
END
WHERE parent < 0
    )", true};
    fix_orphans.run_void();

     // These are all the correctly linked tabs according to the prev field.
     // The next field is ignored.
    State<>::Ment<> make_siblings {R"(
CREATE TEMPORARY TABLE siblings AS
WITH RECURSIVE sibs (up, left, right) AS (
    SELECT parent, 0, id FROM tabs a
        WHERE closed_at IS NULL AND prev = 0
        AND NOT EXISTS (
            SELECT 1 FROM tabs b
            WHERE b.parent = a.parent AND b.prev = 0 AND b.created_at < a.created_at
        )
    UNION ALL
    SELECT parent, prev, id FROM tabs, sibs
        WHERE closed_at IS NULL AND parent = up AND prev = right
) SELECT up, left, right FROM sibs
    )", true};
    make_siblings.run_void();

    State<>::Ment<> add_final_siblings {R"(
INSERT INTO siblings (up, left, right)
SELECT up, right, 0 FROM siblings a
WHERE NOT EXISTS (SELECT 1 FROM siblings b WHERE b.left = a.right)
    )", true};
    add_final_siblings.run_void();

    State<>::Ment<> fix_siblings {R"(
UPDATE tabs SET next = (SELECT right FROM siblings WHERE left = id)
WHERE EXISTS (SELECT right FROM siblings WHERE left = id)
    )", true};
    fix_siblings.run_void();

    State<int64, int64>::Ment<> get_non_siblings {R"(
SELECT id, parent FROM tabs
WHERE closed_at IS NULL AND id NOT IN (SELECT left FROM siblings)
ORDER BY created_at
    )", true};
    vector<tuple<int64, int64>> non_siblings = get_non_siblings.run();
    LOG("Found non-siblings numbering", non_siblings.size());
    for (auto& pair : non_siblings) {
        LOG("non-sibling", get<0>(pair), get<1>(pair));
        set_parent(get<0>(pair), -1);
        place_tab(get<0>(pair), get<1>(pair), TabRelation::LAST_CHILD);
    }

    State<>::Ment<> drop_siblings {"DROP TABLE siblings", true};
    drop_siblings.run_void();
}

///// WINDOWS

int64 create_window (int64 focused_tab) {
    LOG("create_window", focused_tab);
    Transaction tr;

    static State<>::Ment<int64, double> create {R"(
INSERT INTO windows (focused_tab, created_at) VALUES (?, ?)
    )"};
    create.run_void(focused_tab, now());
    int64 id = sqlite3_last_insert_rowid(db);
    window_updated(id);
    return id;
}

vector<WindowData> get_all_unclosed_windows () {
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
    LOG("set_window_focused_tab", window, tab);
    Transaction tr;

    static State<>::Ment<int64, int64> set {R"(
UPDATE windows SET focused_tab = ? WHERE id = ?
    )"};
    set.run_void(tab, window);
    window_updated(window);
}

int64 get_window_focused_tab (int64 window) {
    LOG("get_window_focused_tab", window);

    static State<int64>::Ment<int64> get {R"(
SELECT focused_tab FROM windows WHERE id = ?
    )"};
    return get.run_single(window);
}

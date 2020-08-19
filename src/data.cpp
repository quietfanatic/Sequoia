#include "data.h"

#include <chrono>
#include <map>
#include <sqlite3.h>

#include "activities.h"
#include "data_init.h"
#include "settings.h"
#include "util/assert.h"
#include "util/db_support.h"
#include "util/hash.h"
#include "util/logging.h"
#include "util/text.h"
#include "Window.h"

using namespace std;
using namespace std::chrono;

///// Misc

static map<int64, TabData> tabs_by_id;
static map<int64, WindowData> windows_by_id;

static double now () {
    return duration<double>(system_clock::now().time_since_epoch()).count();
}

///// Transactions

static std::vector<Observer*>& all_observers () {
    static std::vector<Observer*> all_observers;
    return all_observers;
}
static std::vector<int64> updated_tabs;
static std::vector<int64> updated_windows;

void tab_updated (int64 id) {
    Transaction tr;
    A(id > 0);
    for (auto t : updated_tabs) {
        if (t == id) return;
    }
    updated_tabs.push_back(id);
}
void window_updated (int64 id) {
    Transaction tr;
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
     // Copy the list because an observer can destroy itself
    auto observers_copy = all_observers();
    for (auto o : observers_copy) {
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
            windows_by_id.clear();
        }
        else {
            static State<>::Ment<> commit {"COMMIT"};
            commit.run_void();
            update_observers();
        }
    }
}

Observer::Observer () { all_observers().emplace_back(this); }
Observer::~Observer () {
    for (auto iter = all_observers().begin(); iter != all_observers().end(); iter++) {
        if (*iter == this) {
            all_observers().erase(iter);
            break;
        }
    }
}

///// TAB HELPER STATEMENTS

bool suspend_child_counts = false;

void change_child_count (int64 parent, int64 diff) {
    if (suspend_child_counts) return;
    while (parent > 0) {
        auto data = get_tab_data(parent);
        data->child_count += diff;
        static State<>::Ment<int64, int64> update {R"(
UPDATE tabs SET child_count = child_count + ? WHERE id = ?
        )"};
        update.run_void(diff, parent);
        tab_updated(parent);

        parent = data->parent;
    }
}

///// TABS

int64 create_tab (int64 reference, TabRelation rel, const string& url, const string& title) {
    Transaction tr;
    LOG("create_webpage_tab", reference, uint(rel), url, title);

    int64 parent;
    Bifractor position;
    tie(parent, position) = make_location(reference, rel);

    static State<>::Ment<int64, Bifractor, uint64, string, string, double> create {R"(
INSERT INTO tabs (parent, position, url_hash, url, title, created_at)
VALUES (?, ?, ?, ?, ?, ?)
    )"};
    create.run_void(parent, position, x31_hash(url.c_str()), url, title, now());
    int64 id = sqlite3_last_insert_rowid(db);
    tab_updated(id);

    change_child_count(parent, 1);
    return id;
}

TabData* get_tab_data (int64 id) {
    auto iter = tabs_by_id.find(id);
    if (iter != tabs_by_id.end()) {
        return &iter->second;
    }

    static State<int64, Bifractor, int64, string, string, string, double, double, double, double>
        ::Ment<int64> get {R"(
SELECT parent, position, child_count, url, title, favicon, created_at, visited_at, starred_at, closed_at
FROM tabs WHERE id = ?
    )"};

     // This make_from_tuple does an extra copy, might be worth trying to fix
    auto res = tabs_by_id.emplace(id, make_from_tuple<TabData>(get.run_single(id)));
    return &res.first->second;
}

std::vector<int64> get_all_children (int64 parent) {
    LOG("get_all_children", parent);
    static State<int64>::Ment<int64> get {R"(
SELECT id FROM tabs WHERE parent = ? ORDER BY position
    )"};
    return get.run(parent);
}

std::vector<int64> get_all_unclosed_children (int64 parent) {
    LOG("get_all_unclosed_children", parent);
    static State<int64>::Ment<int64> get {R"(
SELECT id FROM tabs WHERE parent = ? AND closed_at IS NULL ORDER BY position
    )"};
    return get.run(parent);
}

std::vector<int64> get_last_visited_tabs (int n_tabs) {
     // TODO: create index
    LOG("get_last_visited_tabs", n_tabs);
    static State<int64>::Ment<int64> get {R"(
SELECT id FROM tabs WHERE closed_at IS NULL ORDER BY visited_at DESC LIMIT ?
    )"};
    return get.run(n_tabs);
}

string get_tab_url (int64 id) {
    return get_tab_data(id)->url;
}

int64 get_prev_unclosed_tab (int64 id) {
    LOG("get_prev_unclosed_tab", id);

    auto data = get_tab_data(id);
    State<int64>::Ment<int64, Bifractor> get {R"(
SELECT id FROM tabs WHERE parent = ? AND position < ? AND closed_at IS NULL ORDER BY position DESC LIMIT 1
    )"};
    return get.run_or(data->parent, data->position, 0);
}

int64 get_next_unclosed_tab (int64 id) {
    LOG("get_next_unclosed_tab", id);

    auto data = get_tab_data(id);
    State<int64>::Ment<int64, Bifractor> get {R"(
SELECT id FROM tabs WHERE parent = ? AND position > ? AND closed_at IS NULL ORDER BY position ASC LIMIT 1
    )"};
    return get.run_or(data->parent, data->position, 0);
}

void set_tab_url (int64 id, const string& url) {
    LOG("set_tab_url", id, url);
    string utf8_url = make_url_utf8(url);
    auto data = get_tab_data(id);
    if (utf8_url == data->url) return;

    Transaction tr;
    data->url = utf8_url;
    static State<>::Ment<uint64, string, int64> set {R"(
UPDATE tabs SET url_hash = ?, url = ? WHERE id = ?
    )"};
    set.run_void(x31_hash(utf8_url), utf8_url, id);
    tab_updated(id);
}

void set_tab_title (int64 id, const string& title) {
    LOG("set_tab_title", id, title);
    auto data = get_tab_data(id);
    if (title == data->title) return;

    Transaction tr;
    data->title = title;
    static State<>::Ment<string, int64> set {R"(
UPDATE tabs SET title = ? WHERE id = ?
    )"};
    set.run_void(title, id);
    tab_updated(id);
}

void set_tab_favicon (int64 id, const string& favicon) {
    LOG("set_tab_favicon", id, favicon);
    auto data = get_tab_data(id);
    if (favicon == data->favicon) return;

    Transaction tr;
    data->favicon = favicon;
    static State<>::Ment<string, int64> set {R"(
UPDATE tabs SET favicon = ? WHERE id = ?
    )"};
    set.run_void(favicon, id);
    tab_updated(id);
}

void set_tab_visited (int64 id) {
    LOG("set_tab_visited", id);
    Transaction tr;

    double visited_at = now();
    get_tab_data(id)->visited_at = visited_at;
    static State<>::Ment<double, int64> set {R"(
UPDATE tabs SET visited_at = ? WHERE id = ?
    )"};
    set.run_void(visited_at, id);
    tab_updated(id);
}

void set_tab_starred_at (int64 id, optional<double> starred_at) {
    Transaction tr;
    get_tab_data(id)->starred_at = starred_at.value_or(0);
    static State<>::Ment<optional<double>, int64> set {R"(
UPDATE tabs SET starred_at = ? WHERE id = ?
    )"};
    set.run_void(starred_at, id);
    tab_updated(id);
}

void star_tab (int64 id) {
    LOG("star_tab", id);
    set_tab_starred_at(id, now());
}

void unstar_tab (int64 id) {
    LOG("unstar_tab", id);
    set_tab_starred_at(id, nullopt);
}

int64 get_last_closed_tab () {
    LOG("get_last_closed_tab");
    Transaction tr;

    static State<int64>::Ment<> find {R"(
SELECT id FROM tabs WHERE closed_at IS NOT NULL
ORDER BY closed_at DESC LIMIT 1
    )"};
    return find.run_or(0);
}

void set_tab_closed_at (int64 id, optional<double> closed_at) {
    get_tab_data(id)->closed_at = closed_at.value_or(0);
    static State<>::Ment<optional<double>, int64> set {R"(
UPDATE tabs SET closed_at = ? WHERE id = ?
    )"};
    set.run_void(closed_at, id);
    tab_updated(id);
}

void close_tab (int64 id) {
    LOG("close_tab", id);
    Transaction tr;

    auto data = get_tab_data(id);
    if (data->closed_at) return;

    set_tab_closed_at(id, now());
    change_child_count(data->parent, -1 - data->child_count);
    prune_closed_tabs(20, 15*60);

     // If any windows are focusing this tab, have them move their focus
    int64 successor = id;
    for (auto w : get_all_unclosed_windows()) {
        if (get_window_data(w)->focused_tab == id) {
            while (get_tab_data(successor)->closed_at) {
                auto s = get_next_unclosed_tab(successor);
                if (!s) s = get_tab_data(successor)->parent;
                if (!s) s = get_prev_unclosed_tab(successor);
                if (!s) s = create_tab(0, TabRelation::LAST_CHILD, "about:blank");
                successor = s;
            }
             // Auto load successor if closed tab was loaded
             // TODO: Apply preloading here?
            if (auto activity = activity_for_tab(id)) {
                if (activity->window && activity->window->id == w) {
                    activity->window->claim_activity(ensure_activity_for_tab(successor));
                }
            }
            set_window_focused_tab(w, successor);
        }
    }
    delete activity_for_tab(id);
}

void close_tab_with_heritage (int64 id) {
    LOG("close_tab_with_heritage", id);
    Transaction tr;

    vector<int64> children = get_all_unclosed_children(id);
    if (!children.empty()) {
        int64 heir = children[0];
        for (size_t i = 1; i < children.size(); i++) {
            move_tab(children[i], heir, TabRelation::LAST_CHILD);
        }
        move_tab(heir, id, TabRelation::AFTER);
    }
    close_tab(id);
}

void unclose_tab (int64 id) {
    LOG("unclose_tab", id);
    Transaction tr;

    auto data = get_tab_data(id);
    if (!get_tab_data(id)->closed_at) return;
    set_tab_closed_at(id, nullopt);
    change_child_count(data->parent, 1 + data->child_count);
}

void delete_tab_and_children (int64 id) {
    LOG("delete_tab_and_children", id);
    Transaction tr;

    auto data = get_tab_data(id);
    for (int64 child : get_all_children(id)) {
        delete_tab_and_children(child);
    }
    data->deleted = true;
    delete activity_for_tab(id);

    static State<>::Ment<int64> do_it {R"(
DELETE FROM tabs WHERE id = ?
    )"};
    do_it.run_void(id);
    tab_updated(id);
}

void prune_closed_tabs (int64 more_than, double older_than) {
    LOG("prune_closed_tabs", more_than, older_than);
    Transaction tr;

    static State<int64>::Ment<int64, double> find {R"(
SELECT id FROM (
    SELECT id, closed_at FROM tabs
    WHERE closed_at IS NOT NULL
    ORDER BY closed_at DESC LIMIT -1 OFFSET ?
)
WHERE closed_at < ?
    )"};
    auto pruned_tabs = find.run(more_than, now() - older_than);
    for (int64 tab : pruned_tabs) {
        delete_tab_and_children(tab);
    }
}

void move_tab (int64 id, int64 parent, const Bifractor& position) {
    LOG("move_tab", id, parent, position);
    Transaction tr;

    auto data = get_tab_data(id);
    if (!data->closed_at) {
        change_child_count(data->parent, -1 - data->child_count);
    }

    data->parent = parent;
    data->position = position;
    static State<>::Ment<int64, Bifractor, int64> set {R"(
UPDATE tabs SET parent = ?, position = ? WHERE id = ?
    )"};
    set.run_void(parent, position, id);
    tab_updated(id);

    if (!data->closed_at) {
        change_child_count(parent, 1 + data->child_count);
    }
}
void move_tab (int64 id, int64 reference, TabRelation rel) {
    int64 parent;
    Bifractor position;
    tie(parent, position) = make_location(reference, rel);
    move_tab(id, parent, position);
}

pair<int64, Bifractor> make_location (int64 reference, TabRelation rel) {
    switch (rel) {
    case TabRelation::BEFORE: {
        TabData* ref = get_tab_data(reference);
        static State<Bifractor>::Ment<int64, Bifractor> get_prev {R"(
SELECT position FROM tabs WHERE parent = ? AND position < ? ORDER BY position DESC LIMIT 1
        )"};
        Bifractor prev = get_prev.run_or(ref->parent, ref->position, 0);
        return pair(
            ref->parent,
            Bifractor(prev, ref->position, 0xf0)
        );
    }
    case TabRelation::AFTER: {
        TabData* ref = get_tab_data(reference);
        static State<Bifractor>::Ment<int64, Bifractor> get_next {R"(
SELECT position FROM tabs WHERE parent = ? AND position > ? ORDER BY position ASC LIMIT 1
        )"};
        Bifractor next = get_next.run_or(ref->parent, ref->position, 1);
        return pair(
            ref->parent,
            Bifractor(ref->position, next, 0x10)
        );
    }
    case TabRelation::FIRST_CHILD: {
        static State<Bifractor>::Ment<int64> get_first {R"(
SELECT position FROM tabs WHERE parent = ? ORDER BY position ASC LIMIT 1
        )"};
        Bifractor first = get_first.run_or(reference, 1);
        return pair(
            reference,
            Bifractor(0, first, 0xf8)
        );
    }
    case TabRelation::LAST_CHILD: {
        static State<Bifractor>::Ment<int64> get_last {R"(
SELECT position FROM tabs WHERE parent = ? ORDER BY position DESC LIMIT 1
        )"};
        Bifractor last = get_last.run_or(reference, 0);
        return pair(
            reference,
            Bifractor(last, 1, 0x08)
        );
    }
    default: throw std::logic_error("make_location called with invalid TabRelation");
    }
}

///// WINDOWS

int64 create_window (int64 root_tab, int64 focused_tab) {
    LOG("create_window", focused_tab);
    Transaction tr;

    static State<>::Ment<int64, int64, double> create {R"(
INSERT INTO windows (root_tab, focused_tab, created_at) VALUES (?, ?, ?)
    )"};
    create.run_void(root_tab, focused_tab, now());
    int64 id = sqlite3_last_insert_rowid(db);
    window_updated(id);
    return id;
}

WindowData* get_window_data (int64 id) {
    LOG("get_window_data", id);

    auto iter = windows_by_id.find(id);
    if (iter != windows_by_id.end()) {
        return &iter->second;
    }

    static State<int64, int64, int64, double, double>::Ment<int64> get {R"(
SELECT id, root_tab, focused_tab, created_at, closed_at FROM windows WHERE id = ?
    )"};
    auto res = windows_by_id.emplace(id, make_from_tuple<WindowData>(get.run_single(id)));
    return &res.first->second;
}

vector<int64> get_all_unclosed_windows () {
    LOG("get_all_unclosed_windows");

    static State<int64>::Ment<> get {R"(
SELECT id FROM windows WHERE closed_at IS NULL
    )"};
    return get.run();
}

int64 get_last_closed_window () {
    LOG("get_last_closed_window");

    static State<int64>::Ment<> get {R"(
SELECT id FROM windows WHERE closed_at IS NOT NULL ORDER BY closed_at DESC LIMIT 1
    )"};
    return get.run_or(0);
}

void set_window_root_tab (int64 window, int64 tab) {
    LOG("set_window_root_tab", window, tab);
    Transaction tr;

    get_window_data(window)->root_tab = tab;
    static State<>::Ment<int64, int64> set {R"(
UPDATE windows SET root_tab = ? WHERE id = ?
    )"};
    set.run_void(tab, window);
    window_updated(window);
}

void set_window_focused_tab (int64 window, int64 tab) {
    LOG("set_window_focused_tab", window, tab);
    Transaction tr;

    get_window_data(window)->focused_tab = tab;
    static State<>::Ment<int64, int64> set {R"(
UPDATE windows SET focused_tab = ? WHERE id = ?
    )"};
    set.run_void(tab, window);
    window_updated(window);
}

void set_window_closed_at (int64 window, optional<double> closed_at) {
    Transaction tr;
    get_window_data(window)->closed_at = closed_at.value_or(0);
    static State<>::Ment<optional<double>, int64> set {R"(
UPDATE windows SET closed_at = ? WHERE id = ?
    )"};
    set.run_void(closed_at, window);
    window_updated(window);
}

void close_window (int64 window) {
    LOG("close_window", window);
    set_window_closed_at(window, now());
}

void unclose_window (int64 window) {
    LOG("unclose_window", window);
    set_window_closed_at(window, nullopt);
}

///// MISC

void fix_problems () {
    LOG("fix_problems");
    Transaction tr;

    suspend_child_counts = true;

    State<int64>::Ment<> get_orphans {R"(
SELECT id FROM tabs a
WHERE parent <> 0
AND NOT EXISTS (SELECT 1 FROM tabs b WHERE b.id = a.parent)
ORDER BY created_at
    )", true};
    vector<int64> orphans = get_orphans.run();
    if (!orphans.empty()) {
        int64 orphanage = create_tab(0, TabRelation::LAST_CHILD, "data:text/html,<title>Orphaned Tabs</title>", "Orphaned Tabs");
        for (auto orphan : orphans) {
            move_tab(orphan, orphanage, TabRelation::LAST_CHILD);
        }
    }

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

    suspend_child_counts = false;
}


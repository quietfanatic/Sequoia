#include "data.h"

#include <type_traits>
#include <unordered_map>

#include <sqlite3.h>
#include "data_init.h"
#include "util/assert.h"
#include "util/db_support.h"
#include "util/hash.h"
#include "util/json.h"
#include "util/log.h"
#include "util/time.h"

using namespace std;

static Update current_update;

///// Misc

 // Took way too many tries to get this correct
template <class T, class Def = const remove_cvref_t<T>&>
optional<remove_cvref_t<T>> null_default (T&& v, Def def = Def{}) {
    if (v == def) return nullopt;
    else return {forward<T>(v)};
}

///// Pages

unordered_map<PageID, PageData> page_cache;

const PageData* PageData::load (PageID id) {
    LOG("PageData::load", id);
    AA(id > 0);
    PageData& r = page_cache[id];
    if (r.id) {
        AA(r.id == id);
        return &r;
    }
    static State<string, optional<Method>, int64, string, double, string>::Ment<PageID> sel = R"(
SELECT _url, _method, _group, _favicon_url, _visited_at, _title FROM _pages WHERE _id = ?
    )";
     // TODO: Is it possible to avoid the extra copy?
     //  Probably, by simplifying the db support module
    if (auto row = sel.run_optional(id)) {
        r.id = id;
        r.url = get<0>(*row);
        r.method = get<1>(*row).value_or(Method::Unknown);
        r.group = get<2>(*row);
        r.favicon_url = get<3>(*row);
        r.visited_at = get<4>(*row);
        r.title = get<5>(*row);
        r.exists = true;
    }
    else {
        r.exists = false;
    }
    return &r;
}

void PageData::save () {
    LOG("PageData::save", id);
    Transaction tr;
    if (exists) {
        static State<>::Ment<optional<PageID>, int64, string, optional<Method>, optional<int64>, optional<string>, optional<double>, optional<string>> ins = R"(
INSERT OR REPLACE INTO _pages (_id, _url_hash, _url, _method, _group, _favicon_url, _visited_at, _title)
VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        )";
        AA(url != "");
        ins.run_void(
            null_default(id),
            x31_hash(url),
            url,
            null_default(method, Method::Unknown),
            null_default(group),
            null_default(favicon_url),
            null_default(visited_at),
            null_default(title)
        );
        if (!id) id = PageID{sqlite3_last_insert_rowid(db)};
        page_cache[id] = *this;
        updated();
    }
    else if (id) {
        static State<>::Ment<PageID> del = R"(
DELETE FROM _pages WHERE _id = ?
        )";
        del.run_void(id);
        page_cache[id] = *this;
        updated();
    }
}

void PageData::updated () {
    AA(id);
    for (PageID p : current_update.pages) {
        if (p == id) return;
    }
    current_update.pages.push_back(*this);
}

vector<PageID> get_pages_with_url (const string& url) {
    LOG("get_pages_with_url", url);
    static State<PageID>::Ment<int64, string> sel = R"(
SELECT _id FROM _pages WHERE _url_hash = ? AND _url = ?
    )";
    return sel.run(x31_hash(url), url);
}

///// Links

unordered_map<LinkID, LinkData> link_cache;

const LinkData* LinkData::load (LinkID id) {
    LOG("LinkData::load", id);
    AA(id > 0);
    LinkData& r = link_cache[id];
    if (r.id) {
        AA(r.id == id);
        return &r;
    }
    static State<PageID, PageID, PageID, Bifractor, string, double, double>::Ment<LinkID> sel = R"(
SELECT _opener_page, _from_page, _to_page, _position, _title, _created_at, _trashed_at
FROM _links WHERE _id = ?
    )";
    if (auto row = sel.run_optional(id)) {
        r.opener_page = get<0>(*row);
        r.from_page = get<1>(*row);
        r.to_page = get<2>(*row);
        r.position = move(get<3>(*row));
        r.title = move(get<4>(*row));
        r.created_at = get<5>(*row);
        r.trashed_at = get<6>(*row);
        r.exists = true;
    }
    else {
        r.exists = false;
    }
    return &r;
}

void LinkData::save () {
    LOG("LinkData::save", id);
    Transaction tr;
    if (exists) {
        if (!created_at) {
            AA(!id);
            created_at = now();
        }
        static State<>::Ment<
            optional<LinkID>,
            optional<PageID>,
            int64,
            int64,
            Bifractor,
            optional<string>,
            double,
            optional<double>
        > ins = R"(
INSERT OR REPLACE INTO _links (_id, _opener_page, _from_page, _to_page, _position, _title, _created_at, _trashed_at)
VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        )";
        ins.run_void(
            null_default(id),
            null_default(opener_page),
            from_page,
            to_page,
            position,
            null_default(title),
            created_at,
            null_default(trashed_at)
        );
        if (!id) id = LinkID{sqlite3_last_insert_rowid(db)};
        link_cache[id] = *this;
        updated();
    }
    else if (id) {
        static State<>::Ment<LinkID> del = R"(
DELETE FROM _links WHERE _id = ?1;
DELETE FROM _view_links WHERE _link = ?1
        )";
        del.run_single(id);
        link_cache[id] = *this;
        updated();
    }
}

void LinkData::updated () {
    AA(id);
    for (LinkID l : current_update.links) {
        if (l == id) return;
    }
    current_update.links.push_back(*this);
}

void LinkData::move_before (LinkID next_link) {
    LOG("LinkData::move_before", id, next_link);
    static State<PageID, Bifractor, optional<Bifractor>>::Ment<LinkID> sel = R"(
SELECT n._from_page, n._position, (
    SELECT _position FROM _links p
    WHERE p._from_page = n._from_page AND p._position < n._position
    ORDER BY p._position DESC
)
FROM _links n WHERE n._id = ?
    )";
    auto row = sel.run_single(next_link);
    from_page = get<0>(row);
    position = Bifractor(get<2>(row).value_or(0), get<1>(row), 0xc0);
}

void LinkData::move_after (LinkID prev_link) {
    LOG("LinkData::move_after", id, prev_link);
    static State<PageID, Bifractor, optional<Bifractor>>::Ment<LinkID> sel = R"(
SELECT p._from_page, p._position, (
    SELECT _position FROM _links n
    WHERE n._from_page = p._from_page AND n._position > p._position
    ORDER BY n._position DESC
)
FROM _links p WHERE p._id = ?
    )";
    auto row = sel.run_single(prev_link);
    from_page = get<0>(row);
    position = Bifractor(get<1>(row), get<2>(row).value_or(1), 0x40);
}

void LinkData::move_first_child (PageID from_page) {
    LOG("LinkData::move_first_child", id, from_page);
    static State<Bifractor>::Ment<PageID> sel = R"(
SELECT _position FROM _links WHERE _from_page = ?
ORDER BY _position ASC LIMIT 1
    )";
    auto row = sel.run_optional(from_page);
    this->from_page = from_page;
    position = Bifractor(0, row.value_or(1), 0xf8);
}

void LinkData::move_last_child (PageID from_page) {
    LOG("LinkData::move_last_child", id, from_page);
    static State<Bifractor>::Ment<PageID> sel = R"(
SELECT _position FROM _links WHERE _from_page = ?
ORDER BY _position DESC LIMIT 1
    )";
    auto row = sel.run_optional(from_page);
    this->from_page = from_page;
    position = Bifractor(row.value_or(0), 1, 0x08);
}

vector<LinkID> get_links_from_page (PageID from_page) {
    LOG("get_links_from_page", from_page);
    AA(from_page);
    static State<LinkID>::Ment<PageID> sel = R"(
SELECT _id FROM _links WHERE _from_page = ?
    )";
    return sel.run(from_page);
}

vector<LinkID> get_links_to_page (PageID to_page) {
    LOG("get_links_to_page", to_page);
    AA(to_page);
    static State<LinkID>::Ment<PageID> sel = R"(
SELECT _id FROM _links WHERE _to_page = ?
    )";
    return sel.run(to_page);
}

///// Views

unordered_map<ViewID, ViewData> view_cache;

const ViewData* ViewData::load (ViewID id) {
    LOG("ViewData::load", id);
    AA(id > 0);
    ViewData& r = view_cache[id];
    if (r.id) {
        AA(r.id == id);
        return &r;
    }
    static State<PageID, LinkID, double, double, string>::Ment<ViewID> sel = R"(
SELECT _root_page, _focused_tab, _closed_at, _trashed_at, _expanded_tabs FROM _views WHERE _id = ?
    )";
    if (auto row = sel.run_optional(id)) {
        r.id = id;
        r.root_page = get<0>(*row);
        r.focused_tab = get<1>(*row);
        r.closed_at = get<2>(*row);
        r.trashed_at = get<3>(*row);
        for (int64 l : json::Array(json::parse(get<4>(*row)))) {
            r.expanded_tabs.insert(LinkID{l});
        }
        r.exists = true;
    }
    else {
        r.exists = false;
    }
    return &r;
}

void ViewData::save () {
    LOG("ViewData::save", id);
    Transaction tr;
    if (exists) {
        static State<>::Ment<optional<ViewID>, PageID, LinkID, optional<double>, optional<double>, string> ins = R"(
INSERT OR REPLACE INTO _views (_id, _root_page, _focused_tab, _closed_at, _trashed_at, _expanded_tabs)
VALUES (?, ?, ?, ?, ?, ?)
        )";
        json::Array expanded_tabs_json;
        expanded_tabs_json.reserve(expanded_tabs.size());
        for (LinkID tab : expanded_tabs) expanded_tabs_json.push_back(int64{tab});
        ins.run(
            null_default(id),
            root_page,
            focused_tab,
            null_default(closed_at),
            null_default(trashed_at),
            json::stringify(expanded_tabs_json)
        );
        if (!id) id = ViewID{sqlite3_last_insert_rowid(db)};
        view_cache[id] = *this;
        updated();
    }
    else if (id) {
        static State<>::Ment<ViewID> del = R"(
DELETE FROM _views WHERE _id = ?
        )";
        del.run_void(id);
        view_cache[id] = *this;
        updated();
    }
}

void ViewData::updated () {
    AA(id);
    for (ViewID v : current_update.views) {
        if (v == id) return;
    }
    current_update.views.emplace_back(*this);
}

vector<ViewID> get_open_views () {
    LOG("get_open_views");
    static State<ViewID>::Ment<> sel = R"(
SELECT _id FROM _views WHERE _closed_at IS NULL
    )";
    return sel.run();
}

ViewID get_last_closed_view () {
    LOG("get_last_closed_view");
    static State<ViewID>::Ment<> sel = R"(
SELECT _id FROM _views
WHERE _closed_at IS NOT NULL ORDER BY _closed_at DESC LIMIT 1
    )";
    ViewData r;
    return sel.run_optional().value_or(ViewID{});
}

///// Transactions

static vector<Observer*>& all_observers () {
    static vector<Observer*> all_observers;
    return all_observers;
}

static void update_observers () {
     // All this flag fiddling is to make sure updates are sequenced,
     // so Observer_after_commit doesn't have to be reentrant.
    static bool updating = false;
    static bool again = false;
    if (updating) {
        again = true;
        return;
    }
    updating = true;
    Update update = move(current_update);
     // Copy the list because an observer can destroy itself
    auto observers_copy = all_observers();
    for (auto o : observers_copy) {
        o->Observer_after_commit(update);
    }
    updating = false;
    if (again) {
        again = false;
        update_observers();
    }
}

static size_t transaction_depth = 0;

Transaction::Transaction () {
     // Don't start a transaction in an exception handler lol
    AA(!uncaught_exceptions());
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
            current_update = Update{};
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


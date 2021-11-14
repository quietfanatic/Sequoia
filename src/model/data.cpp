#include "data.h"

#include <type_traits>
#include <unordered_map>

#include <sqlite3.h>
#include "../util/assert.h"
#include "../util/db_support.h"
#include "../util/hash.h"
#include "../util/json.h"
#include "../util/log.h"
#include "../util/time.h"
#include "database.h"
#include "transaction.h"

namespace model {

using namespace std;

///// Misc

 // Took way too many tries to get this correct
template <class T, class Def = const remove_cvref_t<T>&>
optional<remove_cvref_t<T>> null_default (T&& v, Def def = Def{}) {
    if (v == def) return nullopt;
    else return {forward<T>(v)};
}

///// Pages

unordered_map<PageID, Page> page_cache;

const Page* Page::load (PageID id) {
    AA(id > 0);
    Page& r = page_cache[id];
    if (r.id) {
        AA(r.id == id);
        return &r;
    }
    LOG("Page::load", id);
    static State<string, Method, int64, string, double, string>::Ment<PageID> sel = R"(
SELECT _url, _method, _group, _favicon_url, _visited_at, _title FROM _pages WHERE _id = ?
    )";
     // TODO: Is it possible to avoid the extra copy?
     //  Probably, by simplifying the db support module
    if (auto row = sel.run_optional(id)) {
        r.id = id;
        r.url = get<0>(*row);
        r.method = get<1>(*row);
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

void Page::save () {
    LOG("Page::save", id);
    Transaction tr;
    if (exists) {
        static State<>::Ment<optional<PageID>, int64, string, Method, int64, string, double, string> ins = R"(
INSERT OR REPLACE INTO _pages (_id, _url_hash, _url, _method, _group, _favicon_url, _visited_at, _title)
VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        )";
        AA(url != "");
        ins.run_void(
            null_default(id),
            x31_hash(url),
            url,
            method,
            group,
            favicon_url,
            visited_at,
            title
        );
        if (!id) id = PageID{sqlite3_last_insert_rowid(db)};
        page_cache[id] = *this;
        updated();
    }
    else {
        AA(id > 0);
        static State<>::Ment<PageID> del = R"(
DELETE FROM _pages WHERE _id = ?
        )";
        del.run_void(id);
    }
    page_cache[id] = *this;
    updated();
}

void Page::updated () {
    AA(id);
    current_update.pages.insert(*this);
}

vector<PageID> get_pages_with_url (const string& url) {
    LOG("get_pages_with_url", url);
    static State<PageID>::Ment<int64, string> sel = R"(
SELECT _id FROM _pages WHERE _url_hash = ? AND _url = ?
    )";
    return sel.run(x31_hash(url), url);
}

///// Links

unordered_map<LinkID, Link> link_cache;

const Link* Link::load (LinkID id) {
    AA(id > 0);
    Link& r = link_cache[id];
    if (r.id) {
        AA(r.id == id);
        return &r;
    }
    LOG("Link::load", id);
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

void Link::save () {
    LOG("Link::save", id);
    Transaction tr;
    if (exists) {
        if (!created_at) {
            AA(!id);
            created_at = now();
        }
        static State<>::Ment<
            optional<LinkID>,
            PageID,
            int64,
            int64,
            Bifractor,
            string,
            double,
            double
        > ins = R"(
INSERT OR REPLACE INTO _links (_id, _opener_page, _from_page, _to_page, _position, _title, _created_at, _trashed_at)
VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        )";
        ins.run_void(
            null_default(id),
            opener_page,
            from_page,
            to_page,
            position,
            title,
            created_at,
            trashed_at
        );
        if (!id) id = LinkID{sqlite3_last_insert_rowid(db)};
    }
    else {
        AA(id > 0);
        static State<>::Ment<LinkID> del = R"(
DELETE FROM _links WHERE _id = ?1;
DELETE FROM _view_links WHERE _link = ?1
        )";
        del.run_single(id);
    }
    link_cache[id] = *this;
    updated();
}

void Link::updated () {
    AA(id);
    current_update.links.insert(*this);
}

void Link::move_before (LinkID next_link) {
    LOG("Link::move_before", id, next_link);
    static State<PageID, Bifractor, optional<Bifractor>>::Ment<LinkID> sel = R"(
SELECT n._from_page, n._position, (
    SELECT _position FROM _links p
    WHERE p._from_page = n._from_page AND p._position < n._position
    ORDER BY p._position DESC LIMIT 1
)
FROM _links n WHERE n._id = ?
    )";
    auto row = sel.run_single(next_link);
    from_page = get<0>(row);
    position = Bifractor(get<2>(row).value_or(0), get<1>(row), 0xc0);
}

void Link::move_after (LinkID prev_link) {
    LOG("Link::move_after", id, prev_link);
    static State<PageID, Bifractor, optional<Bifractor>>::Ment<LinkID> sel = R"(
SELECT p._from_page, p._position, (
    SELECT _position FROM _links n
    WHERE n._from_page = p._from_page AND n._position > p._position
    ORDER BY n._position DESC LIMIT 1
)
FROM _links p WHERE p._id = ?
    )";
    auto row = sel.run_single(prev_link);
    from_page = get<0>(row);
    position = Bifractor(get<1>(row), get<2>(row).value_or(1), 0x40);
}

void Link::move_first_child (PageID from_page) {
    LOG("Link::move_first_child", id, from_page);
    static State<Bifractor>::Ment<PageID> sel = R"(
SELECT _position FROM _links WHERE _from_page = ?
ORDER BY _position ASC LIMIT 1
    )";
    auto row = sel.run_optional(from_page);
    this->from_page = from_page;
    position = Bifractor(0, row.value_or(1), 0xf8);
}

void Link::move_last_child (PageID from_page) {
    LOG("Link::move_last_child", id, from_page);
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

unordered_map<ViewID, View> view_cache;

const View* View::load (ViewID id) {
    AA(id > 0);
    View& r = view_cache[id];
    if (r.id) {
        AA(r.id == id);
        return &r;
    }
    LOG("View::load", id);
    static State<PageID, LinkID, double, double, double, string>::Ment<ViewID> sel = R"(
SELECT _root_page, _focused_tab, _created_at, _closed_at, _trashed_at, _expanded_tabs FROM _views WHERE _id = ?
    )";
    if (auto row = sel.run_optional(id)) {
        r.id = id;
        r.root_page = get<0>(*row);
        r.focused_tab = get<1>(*row);
        r.created_at = get<2>(*row);
        r.closed_at = get<3>(*row);
        r.trashed_at = get<4>(*row);
        for (int64 l : json::Array(json::parse(get<5>(*row)))) {
            r.expanded_tabs.insert(LinkID{l});
        }
        r.exists = true;
    }
    else {
        r.exists = false;
    }
    return &r;
}

void View::save () {
    LOG("View::save", id);
    Transaction tr;
    if (exists) {
        if (!created_at) {
            AA(!id);
            created_at = now();
        }
        static State<>::Ment<optional<ViewID>, PageID, LinkID, double, double, double, string> ins = R"(
INSERT OR REPLACE INTO _views (_id, _root_page, _focused_tab, _created_at, _closed_at, _trashed_at, _expanded_tabs)
VALUES (?, ?, ?, ?, ?, ?, ?)
        )";
        json::Array expanded_tabs_json;
        expanded_tabs_json.reserve(expanded_tabs.size());
        for (LinkID tab : expanded_tabs) expanded_tabs_json.push_back(int64{tab});
        ins.run(
            null_default(id),
            root_page,
            focused_tab,
            created_at,
            closed_at,
            trashed_at,
            json::stringify(expanded_tabs_json)
        );
        if (!id) id = ViewID{sqlite3_last_insert_rowid(db)};
    }
    else {
        AA(id > 0);
        static State<>::Ment<ViewID> del = R"(
DELETE FROM _views WHERE _id = ?
        )";
        del.run_void(id);
    }
    view_cache[id] = *this;
    updated();
}

void View::updated () {
    AA(id);
    current_update.views.insert(*this);
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
    View r;
    return sel.run_optional().value_or(ViewID{});
}

} // namespace model

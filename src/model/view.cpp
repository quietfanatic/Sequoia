#include "view.h"

#include <memory>
#include <unordered_map>

#include <sqlite3.h>
#include "../util/assert.h"
#include "../util/json.h"
#include "../util/log.h"
#include "../util/time.h"
#include "database.h"
#include "link-internal.h"
#include "page-internal.h"
#include "statement.h"
#include "transaction.h"

namespace model {
inline namespace view {

using namespace std;

unordered_map<ViewID, unique_ptr<ViewData>> view_cache;

static ViewData* load_mut (ViewID id) {
    if (!id) return nullptr;

    auto [iter, emplaced] = view_cache.try_emplace(id, nullptr);
    auto& data = iter->second;
    if (!emplaced) return &*data;

    static Statement st_load (db, R"(
SELECT _root_page, _focused_tab, _created_at, _closed_at, _trashed_at, _expanded_tabs FROM _views WHERE _id = ?
    )"sv);
    UseStatement st (st_load);
    st.params(id);
    if (st.optional()) {
        data = make_unique<ViewData>();
        data->root_page = st[0];
        data->focused_tab = st[1];
        data->created_at = st[2];
        data->closed_at = st[3];
        data->trashed_at = st[4];
        for (int64 l : json::Array(json::parse(st[5]))) {
            data->expanded_tabs.insert(LinkID{l});
        }
    }
    return &*data;
}

const ViewData* load (ViewID id) {
    LOG("load View"sv, id);
    return load_mut(id);
}

vector<ViewID> get_open_views () {
    LOG("get_open_views"sv);
    static Statement st_get_open (db, R"(
SELECT _id FROM _views WHERE _closed_at IS NULL
    )"sv);
    UseStatement st (st_get_open);
    return st.collect<ViewID>();
}

ViewID get_last_closed_view () {
    LOG("get_last_closed_view"sv);
    static Statement st_last_closed (db, R"(
SELECT _id FROM _views
WHERE _closed_at IS NOT NULL ORDER BY _closed_at DESC LIMIT 1
    )"sv);
    UseStatement st (st_last_closed);
    if (st.optional()) return st[0];
    else return ViewID{};
}

static ViewID save (ViewID id, const ViewData* data) {
    AA(data);
    AA(data->created_at);
    Transaction tr;

    json::Array expanded_tabs_json;
    expanded_tabs_json.reserve(data->expanded_tabs.size());
    for (LinkID tab : data->expanded_tabs) {
        expanded_tabs_json.push_back(int64{tab});
    }

    static Statement st_save (db, R"(
INSERT OR REPLACE INTO _views (_id, _root_page, _focused_tab, _created_at, _closed_at, _trashed_at, _expanded_tabs)
VALUES (?, ?, ?, ?, ?, ?, ?)
    )"sv);
    UseStatement st (st_save);
    st.params(
        null_default(id), data->root_page, data->focused_tab,
        data->created_at, data->closed_at, data->trashed_at,
        json::stringify(expanded_tabs_json)
    );
    st.run();
    if (!id) id = ViewID(sqlite3_last_insert_rowid(db));
    updated(id);
    return id;
}

ViewID create_view_and_page (Str url) {
    LOG("create_view_and_page"sv, url);
    Transaction tr;
    auto data = make_unique<ViewData>();
    data->root_page = create_page(url);
    data->focused_tab = LinkID{};
    data->created_at = now();
    auto id = save(ViewID{}, &*data);
    auto [iter, emplaced] = view_cache.try_emplace(id, move(data));
    AA(emplaced);
    return id;
}

void close (ViewID id) {
    LOG("close View"sv, id);
    Transaction tr;
    auto data = load_mut(id);
    data->closed_at = now();
    save(id, data);
}

void unclose (ViewID id) {
    LOG("unclose View"sv, id);
    Transaction tr;
    auto data = load_mut(id);
    data->closed_at = 0;
    save(id, data);
}

void navigate_focused_page (ViewID id, Str url) {
    LOG("navigate_focused_page"sv, id, url);
    Transaction tr;
    auto data = load_mut(id);
    if (PageID page = data->focused_page()) {
        set_url(page, url);
    }
}

void focus_tab (ViewID id, LinkID tab) {
    LOG("focus_tab"sv, id, tab);
    Transaction tr;
    auto data = load_mut(id);
    data->focused_tab = tab;
    save(id, data);
}

void create_and_focus_last_child (ViewID id, LinkID parent_tab, Str url, Str title) {
    LOG("create_and_focus_last_child"sv, id, parent_tab, url);
    Transaction tr;
    auto data = load_mut(id);
    PageID parent_page = parent_tab
        ? parent_page = load_mut(parent_tab)->from_page
        : data->root_page;
    LinkID link = create_last_child(parent_page, url, title);
    data->focused_tab = link;
    data->expanded_tabs.insert(parent_tab);
    save(id, data);
}

void trash_tab (ViewID id, LinkID tab) {
    LOG("trash_tab"sv, id, tab);
    if (tab) trash(tab);
    else close(id);
}

void expand_tab (ViewID id, LinkID tab) {
    LOG("expand_tab"sv, id, tab);
    auto data = load_mut(id);
    data->expanded_tabs.insert(tab);
    save(id, data);
}

void contract_tab (ViewID id, LinkID tab) {
    LOG("contract_tab"sv, id, tab);
    auto data = load_mut(id);
    data->expanded_tabs.erase(tab);
    save(id, data);
}

void set_fullscreen (ViewID id, bool fullscreen) {
    LOG("set_fullscreen", id, fullscreen);
    auto data = load_mut(id);
    data->fullscreen = fullscreen;
    updated(id);
}

void updated (ViewID id) {
    AA(id);
    current_update.views.insert(id);
}

} // namespace view
} // namespace model

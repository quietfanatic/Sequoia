#include "view-internal.h"

#include <memory>
#include <unordered_map>

#include <sqlite3.h>
#include "../../util/error.h"
#include "../../util/json.h"
#include "../../util/log.h"
#include "../../util/time.h"
#include "edge-internal.h"
#include "model-internal.h"
#include "node-internal.h"

using namespace std;

namespace model {

static constexpr Str sql_load = R"(
SELECT _root_node, _focused_tab, _created_at, _closed_at, _trashed_at, _expanded_tabs
FROM _views WHERE _id = ?
)"sv;

ViewData* load_mut (ReadRef model, ViewID id) {
    if (!id) return nullptr;

    auto [iter, emplaced] = model->views.cache.try_emplace(id, nullptr);
    auto& data = iter->second;
    if (!emplaced) return &*data;

    UseStatement st (model->views.st_load);
    st.params(id);
    if (st.step()) {
        data = make_unique<ViewData>();
        data->root_node = st[0];
        data->focused_tab = st[1];
        data->created_at = st[2];
        data->closed_at = st[3];
        data->trashed_at = st[4];
        for (int64 l : json::Array(json::parse(st[5]))) {
            data->expanded_tabs.insert(EdgeID{l});
        }
    }
    return &*data;
}

static constexpr Str sql_get_open = R"(
SELECT _id FROM _views WHERE _closed_at IS NULL
)"sv;

vector<ViewID> get_open_views (ReadRef model) {
    LOG("get_open_views"sv);
    UseStatement st (model->views.st_get_open);
    return st.collect<ViewID>();
}

static constexpr Str sql_last_closed = R"(
SELECT _id FROM _views
WHERE _closed_at IS NOT NULL ORDER BY _closed_at DESC LIMIT 1
)"sv;

ViewID get_last_closed_view (ReadRef model) {
    LOG("get_last_closed_view"sv);
    UseStatement st (model->views.st_last_closed);
    if (st.step()) return st[0];
    else return ViewID{};
}

static constexpr Str sql_save = R"(
INSERT OR REPLACE INTO _views (_id, _root_node, _focused_tab, _created_at, _closed_at, _trashed_at, _expanded_tabs)
VALUES (?, ?, ?, ?, ?, ?, ?)
)"sv;

static ViewID save (WriteRef model, ViewID id, const ViewData* data) {
    AA(data);
    AA(data->created_at);

    json::Array expanded_tabs_json;
    expanded_tabs_json.reserve(data->expanded_tabs.size());
    for (EdgeID tab : data->expanded_tabs) {
        expanded_tabs_json.push_back(int64{tab});
    }

    UseStatement st (model->views.st_save);
    st.params(
        null_default(id), data->root_node, data->focused_tab,
        data->created_at, data->closed_at, data->trashed_at,
        json::stringify(expanded_tabs_json)
    );
    st.run();
    AA(sqlite3_changes(model->db) == 1);
    if (!id) id = ViewID(sqlite3_last_insert_rowid(model->db));
    touch(model, id);
    return id;
}

ViewID create_view_and_node (WriteRef model, Str url) {
    LOG("create_view_and_node"sv, url);
    auto data = make_unique<ViewData>();
    data->root_node = create_node(model, url);
    data->focused_tab = EdgeID{};
    data->created_at = now();
    auto id = save(model, ViewID{}, &*data);
    auto [iter, emplaced] = model->views.cache.try_emplace(id, move(data));
    AA(emplaced);
    return id;
}

void close (WriteRef model, ViewID id) {
    LOG("close View"sv, id);
    auto data = load_mut(model, id);
    data->closed_at = now();
    save(model, id, data);
}

void unclose (WriteRef model, ViewID id) {
    LOG("unclose View"sv, id);
    auto data = load_mut(model, id);
    data->closed_at = 0;
    save(model, id, data);
}

void navigate_focused_node (WriteRef model, ViewID id, Str url) {
    LOG("navigate_focused_node"sv, id, url);
    if (NodeID node = focused_node(model, id)) {
        set_url(model, node, url);
    }
}

void focus_tab (WriteRef model, ViewID id, EdgeID tab) {
    LOG("focus_tab"sv, id, tab);
    auto data = load_mut(model, id);
    data->focused_tab = tab;
    save(model, id, data);
}

void create_and_focus_last_child (WriteRef model, ViewID id, EdgeID parent_tab, Str url, Str title) {
    LOG("create_and_focus_last_child"sv, id, parent_tab, url);
    auto data = load_mut(model, id);
    NodeID parent_node = parent_tab
        ? parent_node = load_mut(model, parent_tab)->from_node
        : data->root_node;
    EdgeID edge = create_last_child(model, parent_node, url, title);
    data->focused_tab = edge;
    data->expanded_tabs.insert(parent_tab);
    save(model, id, data);
}

void trash_tab (WriteRef model, ViewID id, EdgeID tab) {
    LOG("trash_tab"sv, id, tab);
    if (tab) trash(model, tab);
    else close(model, id);
}

void expand_tab (WriteRef model, ViewID id, EdgeID tab) {
    LOG("expand_tab"sv, id, tab);
    auto data = load_mut(model, id);
    data->expanded_tabs.insert(tab);
    save(model, id, data);
}

void contract_tab (WriteRef model, ViewID id, EdgeID tab) {
    LOG("contract_tab"sv, id, tab);
    auto data = load_mut(model, id);
    data->expanded_tabs.erase(tab);
    save(model, id, data);
}

void set_fullscreen (WriteRef model, ViewID id, bool fullscreen) {
    LOG("set_fullscreen", id, fullscreen);
    auto data = load_mut(model, id);
    data->fullscreen = fullscreen;
    touch(model, id);
}

void touch (WriteRef model, ViewID id) {
    model->writes.current_update.views.insert(id);
}

ViewModel::ViewModel (sqlite3* db) :
    st_load(db, sql_load),
    st_get_open(db, sql_get_open),
    st_last_closed(db, sql_last_closed),
    st_save(db, sql_save)
{ }

} // namespace model

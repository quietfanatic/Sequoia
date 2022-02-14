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

static String view_url(ViewID id) {
    return "sequoia:view/" + std::to_string(int64(id));
};

static constexpr Str sql_load = R"(
SELECT _focused_tab, _created_at, _closed_at, _expanded_tabs
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
        data->root_node = get_node_with_url(model, view_url(id));
        AA(data->root_node);
        data->focused_tab = st[0];
        data->created_at = st[1];
        data->closed_at = st[2];
        for (int64 l : json::Array(json::parse(st[4]))) {
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

std::vector<EdgeID> get_toplevel_tabs (ReadRef model, ViewID id) {
    auto data = model/id;
    return get_edges_from_node(model, data->root_node);
}

static constexpr Str sql_save = R"(
INSERT OR REPLACE INTO _views (_id, _focused_tab, _created_at, _closed_at, _expanded_tabs)
VALUES (?, ?, ?, ?, ?)
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
        null_default(id), data->focused_tab,
        data->created_at, data->closed_at,
        json::stringify(expanded_tabs_json)
    );
    st.run();
    AA(sqlite3_changes(model->db) == 1);
    if (!id) id = ViewID(sqlite3_last_insert_rowid(model->db));
    touch(model, id);
    return id;
}

ViewID create_view (WriteRef model) {
    LOG("create_view"sv);
    auto data = make_unique<ViewData>();
    data->focused_tab = EdgeID{};
    data->created_at = now();
    auto id = save(model, ViewID{}, &*data);
    data->root_node = ensure_node_with_url(model, view_url(id));
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

void focus_tab (WriteRef model, ViewID id, EdgeID tab) {
    LOG("focus_tab"sv, id, tab);
    auto data = load_mut(model, id);
    data->focused_tab = tab;
    save(model, id, data);
}

void trash_tab (WriteRef model, ViewID id, EdgeID tab) {
    LOG("trash_tab"sv, id, tab);
    AA(tab);
    trash(model, tab);
     // TODO: close this view if it's the last tab
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

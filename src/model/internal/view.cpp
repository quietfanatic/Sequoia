#include "view-internal.h"

#include <memory>
#include <unordered_map>
#include <sqlite3.h>
#include "../../util/error.h"
#include "../../util/json.h"
#include "../../util/log.h"
#include "../../util/time.h"
#include "edge-internal.h"
#include "node-internal.h"
#include "model-internal.h"

using namespace std;

namespace model {

///// Internal

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
        data = std::make_unique<ViewData>();
        data->root_node = get_node_with_url(model, view_url(id));
        AA(data->root_node);
        data->focused_tab = st[0];
        data->created_at = st[1];
        data->closed_at = st[2];
        for (int64 l : json::Array(json::parse(st[3]))) {
            data->expanded_tabs.insert(EdgeID{l});
        }
    }
    return &*data;
}

static void touch (WriteRef model, ViewID id) {
    model->writes.current_update.views.insert(id);
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
    auto data = make_unique<ViewData>();
    data->focused_tab = EdgeID{};
    data->created_at = now();
    auto id = save(model, ViewID{}, &*data);
    data->root_node = ensure_node_with_url(model, view_url(id));
    auto [iter, emplaced] = model->views.cache.try_emplace(id, move(data));
    AA(emplaced);
    return id;
}

///// Accessors

static constexpr Str sql_get_open = R"(
SELECT _id FROM _views WHERE _closed_at = 0
)"sv;

vector<ViewID> get_open_views (ReadRef model) {
    UseStatement st (model->views.st_get_open);
    return st.collect<ViewID>();
}

static constexpr Str sql_last_closed = R"(
SELECT _id FROM _views
WHERE _closed_at > 0 ORDER BY _closed_at DESC LIMIT 1
)"sv;

ViewID get_last_closed_view (ReadRef model) {
    UseStatement st (model->views.st_last_closed);
    if (st.step()) return st[0];
    else return ViewID{};
}

std::vector<EdgeID> get_top_tabs (ReadRef model, ViewID id) {
    auto data = model/id;
    return get_edges_from_node(model, data->root_node);
}

///// Global Mutators

void open_view_for_urls (WriteRef model, const std::vector<String>& urls) {
    auto view = create_view(model);
    auto data = model/view;
    for (auto& url : urls) {
         // TODO: go through navigation upgrade or address validation?
        auto node = ensure_node_with_url(model, url);
        make_last_child(model, data->root_node, node);
    }
}

void unclose_last_closed_view (WriteRef model) {
    auto view = get_last_closed_view(model);
    if (view) unclose(model, view);
}

void unclose_recently_closed_views (WriteRef model) {
     // TODO actually unclose multiple views
    auto view = get_last_closed_view(model);
    if (view) unclose(model, view);
}

void create_default_view (WriteRef model) {
    auto view = create_view(model);
    auto data = model/view;
    make_last_child(model, data->root_node, NodeID{});
}

///// View Mutators

void close (WriteRef model, ViewID id) {
    auto data = load_mut(model, id);
    data->closed_at = now();
    save(model, id, data);
}

void unclose (WriteRef model, ViewID id) {
    auto data = load_mut(model, id);
    data->closed_at = 0;
    save(model, id, data);
}

void set_fullscreen (WriteRef model, ViewID id, bool fs) {
    auto data = load_mut(model, id);
    data->fullscreen = fs;
    touch(model, id);
}

///// Tab Mutators

void focus_tab (WriteRef model, ViewID id, EdgeID tab) {
    auto data = load_mut(model, id);
    data->focused_tab = tab;
    save(model, id, data);
    focus_activity_for_tab(model, id, tab);
}

void navigate_tab (WriteRef model, ViewID id, EdgeID tab, Str address) {
    navigate_activity_for_tab(model, id, tab, address);
}

void reload_tab (WriteRef model, ViewID id, EdgeID tab) {
    if (auto activity = get_activity_for_edge(model, tab)) {
        reload(model, activity);
    }
}

void stop_tab (WriteRef model, ViewID id, EdgeID tab) {
    if (auto activity = get_activity_for_edge(model, tab)) {
        finished_loading(model, activity);
    }
}

void expand_tab (WriteRef model, ViewID id, EdgeID tab) {
    auto data = load_mut(model, id);
    data->expanded_tabs.insert(tab);
    save(model, id, data);
}

void contract_tab (WriteRef model, ViewID id, EdgeID tab) {
    auto data = load_mut(model, id);
    data->expanded_tabs.erase(tab);
    save(model, id, data);
}

void trash_tab (WriteRef model, ViewID id, EdgeID tab) {
    AA(tab);
    trash(model, tab);
     // TODO: close this view if it's the last tab
}

void move_tab_before (WriteRef model, ViewID id, EdgeID tab, EdgeID next) {
    move_before(model, tab, next);
}

void move_tab_after (WriteRef model, ViewID id, EdgeID tab, EdgeID prev) {
    move_after(model, tab, prev);
}

void move_tab_first_child (WriteRef model, ViewID id, EdgeID tab, EdgeID parent) {
    auto parent_data = model/parent;
    if (parent_data->to_node) {
        move_first_child(model, tab, parent_data->to_node);
    }
}

void move_tab_last_child (WriteRef model, ViewID id, EdgeID tab, EdgeID parent) {
    auto parent_data = model/parent;
    if (parent_data->to_node) {
        move_last_child(model, tab, parent_data->to_node);
    }
}

void new_child_tab (WriteRef model, ViewID id, EdgeID tab) {
    auto edge_data = model/tab;
    AA(edge_data);
    if (edge_data->to_node) {
        auto child = make_last_child(model, edge_data->to_node, NodeID{});
        auto data = load_mut(model, id);
        data->focused_tab = tab;
        save(model, id, data);
        focus_activity_for_tab(model, id, child);
    }
}

ViewModel::ViewModel (sqlite3* db) :
    st_load(db, sql_load),
    st_get_open(db, sql_get_open),
    st_last_closed(db, sql_last_closed),
    st_save(db, sql_save)
{ }

} // namespace model

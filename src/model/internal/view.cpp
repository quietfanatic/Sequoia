#include "view-internal.h"

#include <memory>
#include <unordered_map>

#include <sqlite3.h>
#include "../../util/error.h"
#include "../../util/json.h"
#include "../../util/log.h"
#include "../../util/time.h"
#include "../edge.h"
#include "../node.h"
#include "model-internal.h"

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

static constexpr Str sql_get_open = R"(
SELECT _id FROM _views WHERE _closed_at = 0
)"sv;

vector<ViewID> get_open_views (ReadRef model) {
    LOG("get_open_views"sv);
    UseStatement st (model->views.st_get_open);
    return st.collect<ViewID>();
}

static constexpr Str sql_last_closed = R"(
SELECT _id FROM _views
WHERE _closed_at > 0 ORDER BY _closed_at DESC LIMIT 1
)"sv;

ViewID get_last_closed_view (ReadRef model) {
    LOG("get_last_closed_view"sv);
    UseStatement st (model->views.st_last_closed);
    if (st.step()) return st[0];
    else return ViewID{};
}

std::vector<EdgeID> get_top_tabs (ReadRef model, ViewID id) {
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

#ifndef TAP_DISABLE_TESTS
#include "../../tap/tap.h"

static tap::TestSet tests ("model/view", []{
    using namespace model;
    using namespace tap;
    ModelTestEnvironment env;
    Model& model = *new_model(env.db_path);

    is(get_open_views(model).size(), 0, "No open views yet");
    is(get_last_closed_view(model), ViewID{}, "No closed views yet");

    auto first_view = create_view(write(model));
    auto open_views = get_open_views(model);
    is(open_views.size(), 1, "View starts open");
    is(open_views[0], first_view);
    is(get_last_closed_view(model), ViewID{}, "View hasn't been closed yet");

    doesnt_throw([&]{
        close(write(model), first_view);
    }, "close");
    is(get_open_views(model).size(), 0);
    is(get_last_closed_view(model), first_view);

    doesnt_throw([&]{
        unclose(write(model), first_view);
    }, "unclose");
    open_views = get_open_views(model);
    is(open_views.size(), 1);
    is(open_views[0], first_view);
    is(get_last_closed_view(model), ViewID{});

    const ViewData* first_data = model/first_view;
    ok(first_data->root_node, "View has root_node by default");
    const NodeData* root_data = model/first_data->root_node;
    ok(root_data, "View's initial root_node exists");
    is(root_data->url, "sequoia:view/" + std::to_string(int64(first_view)),
        "root_node has correct URL"
    );

    is(get_top_tabs(model, first_view).size(), 0);
    NodeID nodes [5];
    EdgeID edges [5];
    for (uint i = 0; i < 5; i++) {
        nodes[i] = ensure_node_with_url(
            write(model),
            "http://example.com/#" + std::to_string(i)
        );
        edges[i] = make_last_child(
            write(model),
            first_data->root_node,
            nodes[i],
            "Test edge " + std::to_string(i)
        );
    }
    auto top_tabs = get_top_tabs(model, first_view);
    is(top_tabs.size(), 5, "get_top_tabs");

    is(first_data->focused_tab, EdgeID{}, "No focused tab by default");
    focus_tab(write(model), first_view, edges[2]);
    is(first_data->focused_tab, edges[2], "Focused tab set");

    ok(first_data->expanded_tabs.empty(), "No expanded_tabs yet");
    doesnt_throw([&]{
        expand_tab(write(model), first_view, edges[0]);
        expand_tab(write(model), first_view, edges[1]);
        expand_tab(write(model), first_view, edges[4]);
    }, "expand_tab");
    is(first_data->expanded_tabs.size(), 3);
    is(first_data->expanded_tabs.count(edges[0]), 1);
    is(first_data->expanded_tabs.count(edges[1]), 1);
    is(first_data->expanded_tabs.count(edges[4]), 1);

    doesnt_throw([&]{
        contract_tab(write(model), first_view, edges[0]);
        contract_tab(write(model), first_view, edges[4]);
    }, "contract_tab");
    is(first_data->expanded_tabs.size(), 1);
    is(first_data->expanded_tabs.count(edges[1]), 1);

    ok(!first_data->fullscreen);
    doesnt_throw([&]{
        set_fullscreen(write(model), first_view, true);
    }, "set_fullscreen true");
    ok(first_data->fullscreen);
    doesnt_throw([&]{
        set_fullscreen(write(model), first_view, false);
    }, "set_fullscreen false");
    ok(!first_data->fullscreen);

    delete_model(&model);
    done_testing();
});

#endif

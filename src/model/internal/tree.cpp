#include "tree-internal.h"

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

static String tree_url(TreeID id) {
    return "sequoia:tree/" + std::to_string(int64(id));
};

static constexpr Str sql_load = R"(
SELECT _focused_tab, _created_at, _closed_at, _expanded_tabs
FROM _trees WHERE _id = ?
)"sv;

TreeData* load_mut (ReadRef model, TreeID id) {
    if (!id) return nullptr;

    auto [iter, emplaced] = model->trees.cache.try_emplace(id, nullptr);
    auto& data = iter->second;
    if (!emplaced) return &*data;

    UseStatement st (model->trees.st_load);
    st.params(id);
    if (st.step()) {
        data = std::make_unique<TreeData>();
        data->root_node = get_node_with_url(model, tree_url(id));
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

static void touch (WriteRef model, TreeID id) {
    model->writes.current_update.trees.insert(id);
}

static constexpr Str sql_save = R"(
INSERT OR REPLACE INTO _trees (_id, _focused_tab, _created_at, _closed_at, _expanded_tabs)
VALUES (?, ?, ?, ?, ?)
)"sv;

static TreeID save (WriteRef model, TreeID id, const TreeData* data) {
    AA(data);
    AA(data->created_at);

    json::Array expanded_tabs_json;
    expanded_tabs_json.reserve(data->expanded_tabs.size());
    for (EdgeID tab : data->expanded_tabs) {
        expanded_tabs_json.push_back(int64{tab});
    }

    UseStatement st (model->trees.st_save);
    st.params(
        null_default(id), data->focused_tab,
        data->created_at, data->closed_at,
        json::stringify(expanded_tabs_json)
    );
    st.run();
    AA(sqlite3_changes(model->db) == 1);
    if (!id) id = TreeID(sqlite3_last_insert_rowid(model->db));
    touch(model, id);
    return id;
}

TreeID create_tree (WriteRef model) {
    auto data = make_unique<TreeData>();
    data->focused_tab = EdgeID{};
    data->created_at = now();
    auto id = save(model, TreeID{}, &*data);
    data->root_node = ensure_node_with_url(model, tree_url(id));
    auto [iter, emplaced] = model->trees.cache.try_emplace(id, move(data));
    AA(emplaced);
    return id;
}

void set_focused_tab (WriteRef model, TreeID id, EdgeID tab) {
    auto data = load_mut(model, id);
    data->focused_tab = tab;
    save(model, id, data);
}

///// Accessors

static constexpr Str sql_get_open = R"(
SELECT _id FROM _trees WHERE _closed_at = 0
)"sv;

vector<TreeID> get_open_trees (ReadRef model) {
    UseStatement st (model->trees.st_get_open);
    return st.collect<TreeID>();
}

static constexpr Str sql_last_closed = R"(
SELECT _id FROM _trees
WHERE _closed_at > 0 ORDER BY _closed_at DESC LIMIT 1
)"sv;

TreeID get_last_closed_tree (ReadRef model) {
    UseStatement st (model->trees.st_last_closed);
    if (st.step()) return st[0];
    else return TreeID{};
}

std::vector<EdgeID> get_top_tabs (ReadRef model, TreeID id) {
    auto data = model/id;
    return get_edges_from_node(model, data->root_node);
}

///// Global Mutators

void open_tree_for_urls (WriteRef model, const std::vector<String>& urls) {
    auto tree = create_tree(model);
    auto data = model/tree;
    for (auto& url : urls) {
         // TODO: go through navigation upgrade or address validation?
        auto node = ensure_node_with_url(model, url);
        make_last_child(model, data->root_node, node);
    }
}

void unclose_last_closed_tree (WriteRef model) {
    auto tree = get_last_closed_tree(model);
    if (tree) unclose(model, tree);
}

void unclose_recently_closed_trees (WriteRef model) {
     // TODO actually unclose multiple trees
    auto tree = get_last_closed_tree(model);
    if (tree) unclose(model, tree);
}

void create_default_tree (WriteRef model) {
    auto tree = create_tree(model);
    auto data = model/tree;
    make_last_child(model, data->root_node, NodeID{});
}

///// Tree Mutators

void close (WriteRef model, TreeID id) {
    auto data = load_mut(model, id);
    data->closed_at = now();
    save(model, id, data);
}

void unclose (WriteRef model, TreeID id) {
    auto data = load_mut(model, id);
    data->closed_at = 0;
    save(model, id, data);
}

void set_fullscreen (WriteRef model, TreeID id, bool fs) {
    auto data = load_mut(model, id);
    data->fullscreen = fs;
    touch(model, id);
}

///// Tab Mutators

void focus_tab (WriteRef model, TreeID id, EdgeID tab) {
    set_focused_tab(model, id, tab);
    focus_activity_for_tab(model, id, tab);
}

void navigate_tab (WriteRef model, TreeID id, EdgeID tab, Str address) {
    navigate_activity_for_tab(model, id, tab, address);
}

void reload_tab (WriteRef model, TreeID id, EdgeID tab) {
    if (auto activity = get_activity_for_edge(model, tab)) {
        reload(model, activity);
    }
}

void stop_tab (WriteRef model, TreeID id, EdgeID tab) {
    if (auto activity = get_activity_for_edge(model, tab)) {
        finished_loading(model, activity);
    }
}

void expand_tab (WriteRef model, TreeID id, EdgeID tab) {
    auto data = load_mut(model, id);
    data->expanded_tabs.insert(tab);
    save(model, id, data);
}

void contract_tab (WriteRef model, TreeID id, EdgeID tab) {
    auto data = load_mut(model, id);
    data->expanded_tabs.erase(tab);
    save(model, id, data);
}

void trash_tab (WriteRef model, TreeID id, EdgeID tab) {
    AA(tab);
    trash(model, tab);
     // TODO: close this tree if it's the last tab
}

void star_tab (WriteRef model, TreeID id, EdgeID tab) {
    AA(tab);
    if (auto to_node = (model/tab)->to_node) {
        set_starred_at(model, to_node, now());
    }
}

void unstar_tab (WriteRef model, TreeID id, EdgeID tab) {
    AA(tab);
    if (auto to_node = (model/tab)->to_node) {
        set_starred_at(model, to_node, 0);
    }
}

void move_tab_before (WriteRef model, TreeID id, EdgeID tab, EdgeID next) {
    move_before(model, tab, next);
}

void move_tab_after (WriteRef model, TreeID id, EdgeID tab, EdgeID prev) {
    move_after(model, tab, prev);
}

void move_tab_first_child (WriteRef model, TreeID id, EdgeID tab, EdgeID parent) {
    auto parent_data = model/parent;
    if (parent_data->to_node) {
        move_first_child(model, tab, parent_data->to_node);
    }
}

void move_tab_last_child (WriteRef model, TreeID id, EdgeID tab, EdgeID parent) {
    auto parent_data = model/parent;
    if (parent_data->to_node) {
        move_last_child(model, tab, parent_data->to_node);
    }
}

void new_child_tab (WriteRef model, TreeID id, EdgeID tab) {
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

TreeModel::TreeModel (sqlite3* db) :
    st_load(db, sql_load),
    st_get_open(db, sql_get_open),
    st_last_closed(db, sql_last_closed),
    st_save(db, sql_save)
{ }

} // namespace model

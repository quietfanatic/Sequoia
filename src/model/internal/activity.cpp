#include "activity-internal.h"

#include "../../util/time.h"
#include "../edge.h"
#include "model-internal.h"

using namespace std;

namespace model {

///// Internal

ActivityData* load_mut (ReadRef model, ActivityID id) {
    AA(id);
    auto& a = model->activities;
    auto iter = a.by_id.find(id);
    return iter != a.by_id.end()
        ? &*iter->second
        : nullptr;
}

static void touch (WriteRef model, ActivityID id) {
    model->writes.current_update.activities.insert(id);
}

static ActivityID create (WriteRef model, unique_ptr<ActivityData> data) {
     // TODO: Limit maximum number of activities
    AA(data);
    auto& a = model->activities;
    auto id = ActivityID{a.next_id++};
     // Index
    if (data->node) {
        auto [iter, emplaced] = a.by_node.emplace(data->node, id);
        AA(emplaced);
        data->old_node = data->node;
    }
    else if (data->edge) {
        auto [iter, emplaced] = a.by_nodeless_edge.emplace(data->edge, id);
        AA(emplaced);
        data->old_nodeless_edge = data->edge;
    }
    if (data->tree) {
         // Kick out activity that already has this tree
        auto& existing_id = a.by_tree[data->tree];
        if (existing_id) {
            auto iter = a.by_id.find(existing_id);
            AA(iter != a.by_id.end());
            iter->second->old_tree = TreeID{};
            iter->second->tree = TreeID{};
        }
        existing_id = id;
        data->old_tree = data->tree;
    }
     // Insert
    auto [iter, emplaced] = a.by_id.emplace(id, move(data));
    AA(emplaced);
    touch(model, id);
    return id;
}

static void save (WriteRef model, ActivityID id, ActivityData* data) {
    AA(id);
    AA(data);
#ifndef NDEBUG
    AA(data == load_mut(model, id));
#endif
    auto& a = model->activities;
     // Update by-node index
    if (data->node != data->old_node) {
        if (data->old_node) {
            a.by_node.erase(data->old_node);
        }
        if (data->node) {
            auto [iter, emplaced] = a.by_node.emplace(data->node, id);
            AA(emplaced);
        }
        data->old_node = data->node;
    }
     // Update by-edge index (which is only used if node is 0)
    EdgeID nodeless_edge = data->node ? EdgeID{} : data->edge;
    if (data->old_nodeless_edge != nodeless_edge) {
        if (data->old_nodeless_edge) {
            a.by_nodeless_edge.erase(data->old_nodeless_edge);
        }
        if (nodeless_edge) {
            auto [iter, emplaced] = a.by_nodeless_edge.emplace(nodeless_edge, id);
            AA(emplaced);
        }
        data->old_nodeless_edge = nodeless_edge;
    }
     // Update by-tree index
    if (data->tree != data->old_tree) {
        if (data->old_tree) {
            a.by_tree.erase(data->old_tree);
        }
        if (data->tree) {
            auto [iter, emplaced] = a.by_tree.emplace(data->tree, id);
        }
        data->old_tree = data->tree;
    }
    touch(model, id);
}

///// Accessors

vector<ActivityID> get_activities (ReadRef model) {
    vector<ActivityID> r;
    r.reserve(model->activities.by_id.size());
    for (auto& [id, data] : model->activities.by_id) {
        r.push_back(id);
    }
    return r;
}

static ActivityID get_activity_for_node (ReadRef model, NodeID node) {
    AA(node);
    auto& a = model->activities;
    auto iter = a.by_node.find(node);
    if (iter == a.by_node.end()) return ActivityID{};
    else return iter->second;
}

static ActivityID get_activity_for_nodeless_edge (ReadRef model, EdgeID edge) {
    AA(edge);
    auto& a = model->activities;
    auto iter = a.by_nodeless_edge.find(edge);
    if (iter == a.by_nodeless_edge.end()) return ActivityID{};
    else return iter->second;
}

ActivityID get_activity_for_edge (ReadRef model, EdgeID edge) {
    AA(edge);
    auto edge_data = *model/edge;
    AA(edge_data);
    if (edge_data->to_node) {
        return get_activity_for_node(model, edge_data->to_node);
    }
    else return get_activity_for_nodeless_edge(model, edge);
}

ActivityID get_activity_for_tree (ReadRef model, TreeID tree) {
    AA(tree);
    auto& a = model->activities;
    auto iter = a.by_tree.find(tree);
    if (iter == a.by_tree.end()) return ActivityID{};
    else return iter->second;
}

///// Mutators

void focus_activity_for_tab (WriteRef model, TreeID tree, EdgeID edge) {
    AA(tree);
    AA(edge);
    if (auto node = (*model/edge)->to_node) {
        if (auto id = get_activity_for_node(model, node)) {
             // Activity already exists, so claim it.
            auto data = load_mut(model, id);
            data->edge = edge;
            data->tree = tree;
            save(model, id, data);
        }
        else {
             // Activity doesn't exist for this node, so make one.
            auto data = make_unique<ActivityData>();
            data->node = node;
            data->edge = edge;
            data->tree = tree;
             // Should start loading the node's url.
            data->loading_at = now();
            create(model, move(data));
        }
    }
    else {
         // Node doesn't exist for this edge, so do nothing.
    }
}

void unfocus_activity_for_tab (WriteRef model, TreeID tree, EdgeID edge) {
    AA(tree);
    AA(edge);
    if (ActivityID id = get_activity_for_edge(model, edge)) {
        auto data = load_mut(model, id);
        data->tree = TreeID{};
        save(model, id, data);
    }
}

void navigate_activity_for_tab (
    WriteRef model, TreeID tree, EdgeID edge, Str address
) {
    AA(tree);
    AA(edge);
    AA(!address.empty());
    auto edge_data = *model/edge;
    AA(edge_data);
    if (auto id = get_activity_for_edge(model, edge)) {
        auto data = load_mut(model, id);
        AA(data);
        data->loading_address = address;
        data->loading_at = now();
        save(model, id, data);
    }
    else {
        auto data = make_unique<ActivityData>();
         // This may be 0, which is fine.
        data->node = edge_data->to_node;
        data->edge = edge;
         // Only set tree if it's still focusing this tab
        auto tree_data = *model/tree;
        AA(tree_data);
        if (tree_data->focused_tab == edge) {
            data->tree = tree;
        }
        data->loading_address = address;
        data->loading_at = now();
        create(model, move(data));
    }
}

void reload (WriteRef model, ActivityID id) {
    AA(id);
    auto data = load_mut(model, id);
    AA(data);
    data->reloading = true;
    data->loading_at = now();
    save(model, id, data);
}

void started_loading (WriteRef model, ActivityID id) {
    AA(id);
    auto data = load_mut(model, id);
    AA(data);
    data->loading_at = now();
    save(model, id, data);
}

void finished_loading (WriteRef model, ActivityID id) {
    AA(id);
    auto data = load_mut(model, id);
    AA(data);
     // These probably don't matter, but clear them for cleanliness.
    data->loading_address = ""s;
    data->reloading = false;
     // This matters.
    data->loading_at = 0;
    save(model, id, data);
}

void replace_node (WriteRef model, ActivityID id, Str url) {
    AA(id);
    AA(!url.empty());
    auto data = load_mut(model, id);
     // Don't know what to do if there isn't an edge.
    AA(data->edge);
    if (!data->node) {
        data->node = ensure_node_with_url(model, url);
    }
    set_to_node(model, data->edge, data->node);
    save(model, id, data);
}

void title_changed (WriteRef model, ActivityID id, Str title) {
    AA(id);
    auto data = load_mut(model, id);
    AA(data->node);
    set_title(model, data->node, title);
}

void favicon_url_changed (WriteRef model, ActivityID id, Str url) {
    AA(id);
    auto data = load_mut(model, id);
    AA(data->node);
    set_favicon_url(model, data->node, url);
}

void open_last_child (WriteRef model, ActivityID id, Str url, Str title) {
    AA(id);
    AA(!url.empty());
    auto data = load_mut(model, id);
    AA(data->node);
     // TODO: check duplicates?
    auto child_node = ensure_node_with_url(model, url);
    auto child_edge = make_last_child(model, data->node, child_node, title);
}

void open_first_child (WriteRef model, ActivityID id, Str url, Str title) {
    AA(id);
    AA(!url.empty());
    auto data = load_mut(model, id);
    AA(data->node);
     // TODO: check duplicates?
    auto child_node = ensure_node_with_url(model, url);
    auto child_edge = make_last_child(model, data->node, child_node, title);
}

void open_next_sibling (WriteRef model, ActivityID id, Str url, Str title) {
    AA(id);
    AA(!url.empty());
    auto data = load_mut(model, id);
    if (data->edge) {
        auto child_node = ensure_node_with_url(model, url);
        auto child_edge = make_next_sibling(model, data->edge, child_node, title);
    }
}

void open_prev_sibling (WriteRef model, ActivityID id, Str url, Str title) {
    AA(id);
    AA(!url.empty());
    auto data = load_mut(model, id);
    if (data->edge) {
        auto child_node = ensure_node_with_url(model, url);
        auto child_edge = make_prev_sibling(model, data->edge, child_node, title);
    }
}

void delete_activity (WriteRef model, ActivityID id) {
    AA(id);
    auto data = load_mut(model, id);
    auto& a = model->activities;
    if (data->old_node) {
        a.by_node.erase(data->old_node);
    }
    if (data->old_nodeless_edge) {
        a.by_nodeless_edge.erase(data->old_nodeless_edge);
    }
    if (data->old_tree) {
        a.by_tree.erase(data->old_tree);
    }
    a.by_id.erase(id);
    touch(model, id);
}

} // namespace model

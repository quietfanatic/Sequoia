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
    AA(iter != a.by_id.end());
    return &*iter->second;
}

static void touch (WriteRef model, ActivityID id) {
    model->writes.current_update.activities.insert(id);
}

static ActivityID create (WriteRef model, std::unique_ptr<ActivityData> data) {
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
    if (data->view) {
         // Kick out activity that already has this view
        auto& existing_id = a.by_view[data->view];
        if (existing_id) {
            auto iter = a.by_id.find(existing_id);
            AA(iter != a.by_id.end());
            iter->second->old_view = ViewID{};
            iter->second->view = ViewID{};
        }
        existing_id = id;
        data->old_view = data->view;
    }
     // Insert
    auto [iter, emplaced] = a.by_id.emplace(id, std::move(data));
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
     // Update by-view index
    if (data->view != data->old_view) {
        if (data->old_view) {
            a.by_view.erase(data->old_view);
        }
        if (data->view) {
            auto [iter, emplaced] = a.by_view.emplace(data->view, id);
        }
        data->old_view = data->view;
    }
    touch(model, id);
}

///// Accessors

std::vector<ActivityID> get_activities (ReadRef model) {
    std::vector<ActivityID> r;
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

ActivityID get_activity_for_view (ReadRef model, ViewID view) {
    AA(view);
    auto& a = model->activities;
    auto iter = a.by_view.find(view);
    if (iter == a.by_view.end()) return ActivityID{};
    else return iter->second;
}

///// Mutators

void focus_activity_for_tab (WriteRef model, ViewID view, EdgeID edge) {
    AA(view);
    AA(edge);
    if (auto node = (*model/edge)->to_node) {
        if (auto id = get_activity_for_node(model, node)) {
             // Activity already exists, so claim it.
            auto data = load_mut(model, id);
            data->edge = edge;
            data->view = view;
            save(model, id, data);
        }
        else {
             // Activity doesn't exist for this node, so make one.
            auto data = std::make_unique<ActivityData>();
            data->node = node;
            data->edge = edge;
            data->view = view;
             // Should start loading the node's url.
            data->loading_at = now();
            create(model, std::move(data));
        }
    }
    else {
         // Node doesn't exist for this edge, so do nothing.
    }
}

void unfocus_activity_for_tab (WriteRef model, ViewID view, EdgeID edge) {
    AA(view);
    AA(edge);
    if (ActivityID id = get_activity_for_edge(model, edge)) {
        auto data = load_mut(model, id);
        data->view = ViewID{};
        save(model, id, data);
    }
}

void navigate_activity_for_tab (
    WriteRef model, ViewID view, EdgeID edge, Str address
) {
    AA(view);
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
        auto data = std::make_unique<ActivityData>();
         // This may be 0, which is fine.
        data->node = edge_data->to_node;
        data->edge = edge;
         // Only set view if it's still focusing this tab
        auto view_data = *model/view;
        AA(view_data);
        if (view_data->focused_tab == edge) {
            data->view = view;
        }
        data->loading_address = address;
        data->loading_at = now();
        create(model, std::move(data));
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

static void move_activity (WriteRef model, ActivityID id, NodeID node, EdgeID edge) {
    AA(id);
    AA(node);  // edge can be 0 though
    if (auto existing = get_activity_for_node(model, node)) {
        delete_activity(model, existing);
    }
    auto data = load_mut(model, id);
    data->node = node;
    data->edge = edge;
    save(model, id, data);
}

void url_changed (WriteRef model, ActivityID id, Str url) {
    AA(id);
    AA(!url.empty());
    auto data = load_mut(model, id);
    if (!data->node) {
         // Node doesn't exist, create it.
        AA(data->edge);
        auto node = ensure_node_with_url(model, url);
        new_to_node(model, data->edge, node);
        data->node = node;
        save(model, id, data);
        return;
    }
    //else...
    auto node_data = *model/data->node;
    AA(node_data);
    if (node_data->url == url) {
         // Probably shouldn't happen but whatever
        return;
    }
     // Check parent
    else if (data->edge) {
        auto edge_data = *model/data->edge;
        AA(edge_data);
        AA(edge_data->from_node);
        auto parent_data = *model/edge_data->from_node;
        AA(parent_data);
        if (parent_data->url == url) {
             // TODO: also change edge.  This will require looking at views.
            move_activity(model, id, edge_data->from_node, EdgeID{});
            return;
        }
         // Fall through
    }
     // Check children
    auto child_edges = get_edges_from_node(model, data->node);
     // These will be ordered by position.  Search them backwards.
    for (size_t i = child_edges.size(); i != 0; i--) {
        auto child_edge = child_edges[i-1];
        auto child_edge_data = *model/child_edge;
        AA(child_edge_data);
        if (auto child_node = child_edge_data->to_node) {
            auto child_node_data = *model/child_node;
            AA(child_node_data);
            if (child_node_data->url == url) {
                move_activity(model, id, child_node, child_edge);
                return;
            }
        }
    }
     // No nearby node with this url, make a new one.
    auto new_node = ensure_node_with_url(model, url);
    auto new_edge = make_last_child(model, data->node, new_node);
    move_activity(model, id, new_node, new_edge);
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
    if (data->old_view) {
        a.by_view.erase(data->old_view);
    }
    a.by_id.erase(id);
    touch(model, id);
}

} // namespace model

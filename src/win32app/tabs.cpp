#include "tabs.h"

#include "../model/activity.h"
#include "../model/edge.h"
#include "../model/node.h"
#include "../model/view.h"
#include "app.h"

namespace win32app {

using namespace std;

static void gen_tabs (
    App& app, TabTree& tabs, const model::ViewData& view,
    model::EdgeID edge, model::EdgeID parent
) {
    AA(edge);
    auto edge_data = app.model/edge;
    auto node = edge_data->to_node;
    auto node_data = app.model/node;
    auto children = get_edges_from_node(app.model, node);

    int flags = 0;
    if (view.focused_tab == edge) flags |= Tab::FOCUSED;
    auto activity_id = get_activity_for_edge(app.model, edge);
    if (activity_id) {
        auto activity_data = app.model/activity_id;
        if (activity_data->loading_at) flags |= Tab::LOADING;
        else flags |= Tab::LOADED;
    }
    if (node_data) {
        if (node_data->visited_at) flags |= Tab::VISITED;
        if (children.size()) flags |= Tab::EXPANDABLE;
    }
    if (view.expanded_tabs.count(edge)) flags |= Tab::EXPANDED;
    if (edge_data->trashed_at) flags |= Tab::TRASHED;

    tabs.emplace(edge, Tab(node, parent, Tab::Flags(flags)));
    if (view.expanded_tabs.count(edge)) {
        for (model::EdgeID child : children) {
            gen_tabs(app, tabs, view, child, edge);
        }
    }
}

TabTree create_tab_tree (App& app, model::ViewID view) {
    TabTree r;
    auto view_data = app.model/view;
    auto top_edges = get_edges_from_node(app.model, view_data->root_node);
    for (auto edge : top_edges) {
        gen_tabs(app, r, *view_data, edge, model::EdgeID{});
    }
    return r;
}

TabChanges get_changed_tabs (
    const model::Update& update,
    const TabTree& old_tabs, const TabTree& new_tabs
) {
    TabChanges r;
     // Remove tabs that are no longer visible
    for (auto& [id, tab] : old_tabs) {
        if (!new_tabs.count(id)) {
            r.emplace_back(id, std::nullopt);
        }
    }
     // Add tabs that:
     //   - Are newly visible
     //   - Are visible and are in the Update
     //   - Have had their flags changed, most of which won't be reflected in the Update.
    for (auto& [id, tab] : new_tabs) {
        auto old = old_tabs.find(id);
        if (old == old_tabs.end()
            || tab.flags != old->second.flags
            || update.edges.count(id)
            || update.nodes.count(tab.node)
        ) {
            r.emplace_back(id, tab);
        }
    }
    return r;
}

json::Array make_tab_json (
    const model::Model& model,
    model::EdgeID edge, const Tab* tab
) {
    AA(edge);
    auto edge_data = model/edge;
    if (!edge_data || !tab) {
        return json::array(edge);
    }
    if (tab->node) {
        auto node_data = model/tab->node;
        string title = node_data->title;
        if (title.empty()) title = edge_data->title;
        if (title.empty()) title = node_data->url;

        return json::array(
            edge,
            tab->parent,
            edge_data->position.hex(),
            node_data->url,
            node_data->favicon_url,
            title,
            tab->flags
        );
    }
    else {
        return json::array(
            edge,
            tab->parent,
            edge_data->position.hex(),
            ""s, ""s, ""s,
            tab->flags
        );
    }
}

} // namespace model

#include "tabs.h"

#include "node.h"
#include "link.h"
#include "view.h"

namespace model {

using namespace std;

static void gen_tabs (
    ReadRef model,
    unordered_map<LinkID, Tab>& tabs, const ViewData& view,
    LinkID link, NodeID node, LinkID parent
) {
    auto children = get_links_from_node(model, node);
    auto node_data = model/node;
    auto link_data = model/link;

    int flags = 0;
    if (view.focused_tab == link) flags |= Tab::FOCUSED;
    if (node_data->visited_at) flags |= Tab::VISITED;
    if (node_data->state == NodeState::LOADING) flags |= Tab::LOADING;
    if (node_data->state == NodeState::LOADED) flags |= Tab::LOADED;
    if (link && link_data->trashed_at) flags |= Tab::TRASHED;
    if (children.size()) flags |= Tab::EXPANDABLE; // TODO: don't use if inverted
    if (view.expanded_tabs.count(link)) flags |= Tab::EXPANDED;

    tabs.emplace(link, Tab{node, parent, Tab::Flags(flags)});
    if (view.expanded_tabs.count(link)) {
         // TODO: include get_links_to_node
        for (LinkID child : children) {
            gen_tabs(model, tabs, view, child, (model/child)->to_node, link);
        }
    }
}

TabTree create_tab_tree (ReadRef model, ViewID view) {
    TabTree r;
    auto view_data = model/view;
    gen_tabs(model, r, *view_data, LinkID{}, view_data->root_node, LinkID{});
    return r;
}

TabChanges get_changed_tabs (
    const Update& update,
    const TabTree& old_tabs, const TabTree& new_tabs
) {
    TabChanges r;
     // Remove tabs that are no longer visible
    for (auto& [id, tab] : old_tabs) {
        // Using Tab{} causes MSVC to say things like "no appropriate default
        // constructor available" and "Invalid aggregate intitialization".
        // Compiler bug?
        Tab empty;
        if (!new_tabs.count(id)) {
            r.emplace_back(id, empty);
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
            || update.links.count(id)
            || update.nodes.count(tab.node)
        ) {
            r.emplace_back(id, tab);
        }
    }
    return r;
}

} // namespace model

#include "tabs.h"

namespace model {

using namespace std;

static void gen_tabs (
    unordered_map<LinkID, Tab>& tabs, const View& view,
    LinkID link, PageID page, LinkID parent
) {
    auto children = get_links_from_page(page);

    int flags = 0;
    if (view.focused_tab == link) flags |= Tab::FOCUSED;
    if (page->visited_at) flags |= Tab::VISITED;
    if (page->loading) flags |= Tab::LOADING;
    if (page->loaded && !page->loading) flags |= Tab::LOADED;
    if (link && link->trashed_at) flags |= Tab::TRASHED;
    if (children.size()) flags |= Tab::EXPANDABLE; // TODO: don't use if inverted
    if (view.expanded_tabs.count(link)) flags |= Tab::EXPANDED;

    tabs.emplace(link, Tab{page, parent, Tab::Flags(flags)});
    if (view.expanded_tabs.count(link)) {
         // TODO: include get_links_to_page
        for (LinkID child : children) {
            gen_tabs(tabs, view, child, child->to_page, link);
        }
    }
}

TabTree create_tab_tree (const View& view) {
    TabTree r;
    gen_tabs(r, view, LinkID{}, view.root_page, LinkID{});
    return r;
}

TabChanges get_changed_tabs (
    const TabTree& old_tabs, const TabTree& new_tabs, const Update& update
) {
    TabChanges r;
     // Remove tabs that are no longer visible
    for (auto& [id, tab] : old_tabs) {
        // Using Tab{} causes MSVC to say things like "no appropriate default
        //  constructor available" and "Invalid aggregate intitialization".
        //  Compiler bug?
        Tab empty;
        if (!new_tabs.count(id)) {
            r.emplace_back(id, empty);
        }
    }
     // Add tabs that:
     //  - Are newly visible
     //  - Are visible and are in the Update
     //  - Have had their flags changed, most of which won't be reflected in the Update.
    for (auto& [id, tab] : new_tabs) {
        auto old = old_tabs.find(id);
        if (old == old_tabs.end()
            || tab.flags != old->second.flags
            || update.links.count(id)
            || update.pages.count(tab.page)
        ) {
            r.emplace_back(id, tab);
        }
    }
    return r;
}

} // namespace model

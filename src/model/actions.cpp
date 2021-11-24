#include "actions.h"

#include "../util/hash.h"
#include "../util/time.h"
#include "../model/link.h"
#include "../model/page.h"
#include "../model/transaction.h"

namespace model {

using namespace std;

///// Page-focused actions

static PageID create_page (Str url) {
    PageData data;
    data.url = url;
    data.save();
    return data.id;
}

void change_page_url (PageID page, Str url) {
    PageData data = *page;
    data.url = url;
    data.save();
}
void change_page_favicon_url (PageID page, Str url) {
    PageData data = *page;
    data.favicon_url = url;
    data.save();
}
void change_page_title (PageID page, Str title) {
    PageData data = *page;
    data.title = title;
    data.save();
}
void change_page_visited (PageID page) {
    PageData data = *page;
    data.visited_at = now();
    data.save();
}
void start_loading_page (PageID page) {
    PageData data = *page;
    data.loaded = true;
    data.loading = true;
    data.updated();
}
void finish_loading_page (PageID page) {
    PageData data = *page;
    data.loaded = true;
    data.loading = false;
    data.updated();
}
void unload_page (PageID page) {
    PageData data = *page;
    data.loaded = false;
    data.loading = false;
    data.updated();
}

///// Link-focused actions

void open_as_first_child (PageID parent, Str url, Str title) {
    Transaction tr;
    PageID child = create_page(url);
    LinkData link;
    link.opener_page = parent;
    link.from_page = parent;
    link.move_first_child(parent);
    link.to_page = child;
    link.title = title;
    link.save();
}
void open_as_last_child (PageID parent, Str url, Str title) {
    Transaction tr;
    PageID child = create_page(url);
    LinkData link;
    link.opener_page = parent;
    link.from_page = parent;
    link.move_last_child(parent);
    link.to_page = child;
    link.title = title;
    link.save();
}
void open_as_next_sibling (PageID opener, LinkID prev, Str url, Str title) {
    Transaction tr;
    PageID child = create_page(url);
    LinkData link;
    link.opener_page = opener;
    link.from_page = prev->from_page;
    link.move_after(prev);
    link.to_page = child;
    link.title = title;
    link.save();
}
void open_as_prev_sibling (PageID opener, LinkID next, Str url, Str title) {
    Transaction tr;
    PageID child = create_page(url);
    LinkData link;
    link.opener_page = opener;
    link.from_page = next->from_page;
    link.move_before(next);
    link.to_page = child;
    link.title = title;
    link.save();
}

void trash_link (LinkID link) {
     // TODO: unfocus tab in any views that are focusing it
    LinkData data = *link;
    data.trashed_at = now();
    data.save();
}
void delete_link (LinkID link) {
     // TODO: unfocus tab in any views that are focusing it
    LinkData data = *link;
    if (data.trashed_at) {
        data.exists = false;
        data.save();
    }
}
void move_link_before (LinkID link, LinkID next) {
    LinkData data = *link;
    data.move_before(next);
    data.save();
}
void move_link_after (LinkID link, LinkID prev) {
    LinkData data = *link;
    data.move_after(prev);
    data.save();
}
void move_link_first_child (LinkID link, PageID parent) {
    LinkData data = *link;
    data.move_first_child(parent);
    data.save();
}
void move_link_last_child (LinkID link, PageID parent) {
    LinkData data = *link;
    data.move_last_child(parent);
    data.save();
}

///// View-focused actions

void new_view_with_new_page (Str url) {
    Transaction tr;
    ViewData data;
    data.root_page = create_page(url);
    data.save();
}
void close_view (ViewID view) {
    ViewData data = *view;
    data.closed_at = now();
    data.save();
}
void unclose_view (ViewID view) {
    ViewData data = *view;
    data.closed_at = 0;
    data.save();
}

void navigate_focused_page (ViewID view, Str url) {
    if (PageID page = view->focused_page()) {
        change_page_url(page, url);
    }
}

void focus_tab (ViewID view, LinkID tab) {
    model::ViewData data = *view;
    data.focused_tab = tab;
    data.save();
}

void open_as_last_child_in_view (ViewID view, LinkID parent_tab, Str url) {
    model::PageID parent_page = parent_tab ? parent_tab->to_page : view->root_page;
    model::Transaction tr;
    model::PageData child;
    child.url = url;
    child.save();
    model::LinkData link;
    link.opener_page = parent_page;
    link.from_page = parent_page;
    link.to_page = child;
    link.move_last_child(parent_page);
    link.save();
    model::ViewData new_view = *view;
    new_view.focused_tab = link;
    new_view.expanded_tabs.insert(parent_tab);
    new_view.save();
}

void trash_tab (ViewID view, LinkID tab) {
    if (tab) trash_link(tab);
    else close_view(view);
}
void delete_tab (ViewID view, LinkID tab) {
    if (tab) delete_link(tab);
    else close_view(view);
}

void expand_tab (ViewID view, LinkID tab) {
    model::ViewData data = *view;
    data.expanded_tabs.insert(tab);
    data.save();
}
void contract_tab (ViewID view, LinkID tab) {
    model::ViewData data = *view;
    data.expanded_tabs.erase(tab);
    data.save();
}

void change_view_fullscreen (ViewID view, bool fullscreen) {
    model::ViewData data = *view;
    data.fullscreen = fullscreen;
    data.updated();
}

} // namespace model

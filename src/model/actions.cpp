#include "actions.h"

#include "../util/time.h"
#include "link.h"
#include "page.h"
#include "transaction.h"

namespace model {

using namespace std;

static Page create_page (const string& url) {
    Transaction tr;
    Page page;
    page.url = url;
    page.save();
    return page;
}

void open_as_first_child (
    PageID parent, const string& url, const string& title
) {
    Transaction tr;
    Page child = create_page(url);
    Link link;
    link.opener_page = parent;
    link.from_page = parent;
    link.move_first_child(parent);
    link.to_page = child;
    link.title = title;
    link.save();
}
void open_as_last_child (
    PageID parent, const string& url, const std::string& title
) {
    Transaction tr;
    Page child = create_page(url);
    Link link;
    link.opener_page = parent;
    link.from_page = parent;
    link.move_last_child(parent);
    link.to_page = child;
    link.title = title;
    link.save();
}
void open_as_next_sibling (
    PageID opener, LinkID prev, const string& url, const std::string& title
) {
    Transaction tr;
    Page child = create_page(url);
    Link link;
    link.opener_page = opener;
    link.from_page = prev->from_page;
    link.move_after(prev);
    link.to_page = child;
    link.title = title;
    link.save();
}
void open_as_prev_sibling (
    PageID opener, LinkID next, const string& url, const std::string& title
) {
    Transaction tr;
    Page child = create_page(url);
    Link link;
    link.opener_page = opener;
    link.from_page = next->from_page;
    link.move_before(next);
    link.to_page = child;
    link.title = title;
    link.save();
}

void enter_fullscreen (const View& view) {
    AA(false);
}
void leave_fullscreen (const View& view) {
    AA(false);
}

void change_page_url (PageID page, const string& url) {
    Page data = *page;
    data.url = url;
    data.save();
}

void change_page_title (PageID page, const string& title) {
    Page data = *page;
    data.title = title;
    data.save();
}
void change_page_favicon (PageID page, const string& url) {
    Page data = *page;
    data.favicon_url = url;
    data.save();
}

void set_page_visited (PageID page) {
    Page data = *page;
    data.visited_at = now();
    data.save();
}

} // namespace model

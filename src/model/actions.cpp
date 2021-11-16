#include "actions.h"

#include "../util/hash.h"
#include "link.h"
#include "page.h"
#include "transaction.h"

namespace model {

using namespace std;

void open_as_first_child (
    PageID parent, const string& url, const string& title
) {
    Transaction tr;
    PageID child = create_page(url);
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
    PageID child = create_page(url);
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
    PageID child = create_page(url);
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
    PageID child = create_page(url);
    Link link;
    link.opener_page = opener;
    link.from_page = next->from_page;
    link.move_before(next);
    link.to_page = child;
    link.title = title;
    link.save();
}

void message_from_page (PageID page, const json::Value& message) {
    const string& command = message[0];

    switch (x31_hash(command)) {
    case x31_hash("favicon"): {
        page->change_favicon_url(message[1]);
        break;
    }
    case x31_hash("click_link"): {
        std::string url = message[1];
        std::string title = message[2];
        int button = message[3];
        bool double_click = message[4];
        bool shift = message[5];
        bool alt = message[6];
        bool ctrl = message[7];
        if (button == 1) {
//            if (double_click) {
//                if (last_created_new_child && window) {
//                     // TODO: figure out why the new tab doesn't get loaded
//                    set_window_focused_tab(window->id, last_created_new_child);
//                }
//            }
            if (alt && shift) {
                // TODO: get this activity's link id from view
//                link.move_before(page);
            }
            else if (alt) {
//                link.move_after(page);
            }
            else if (shift) {
                model::open_as_first_child(page, url, title);
            }
            else {
                model::open_as_last_child(page, url, title);
            }
        }
        break;
    }
    case x31_hash("new_children"): {
     // TODO
//        last_created_new_child = 0;
//        const json::Array& children = message[1];
//        Transaction tr;
//        for (auto& child : children) {
//            const string& url = child[0];
//            const string& title = child[1];
//            create_tab(tab, TabRelation::LAST_CHILD, url, title);
//        }
        break;
    }
    default: {
        throw logic_error("Unknown message name");
    }
    }
}

} // namespace model
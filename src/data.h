#pragma once

#include <string>
#include <vector>

#include "util/bifractor.h"
#include "util/types.h"

///// TABS

enum class TabRelation {
    BEFORE,
    AFTER,
    FIRST_CHILD,
    LAST_CHILD
};

struct TabData {
    int64 parent;
    Bifractor position;
    int64 child_count;
    std::string url;
    std::string title;
    std::string favicon;
    double created_at;
    double visited_at;
    double starred_at;
    double closed_at;
    bool deleted = false;
    TabData(
        int64 parent,
        const Bifractor& position,
        int64 child_count,
        const std::string& url,
        const std::string& title,
        const std::string& favicon,
        double created_at,
        double visited_at,
        double starred_at,
        double closed_at
    ) :
        parent(parent),
        position(position),
        child_count(child_count),
        url(url),
        title(title),
        favicon(favicon),
        created_at(created_at),
        visited_at(visited_at),
        starred_at(starred_at),
        closed_at(closed_at)
    { }
};

int64 create_tab (
    int64 reference,
    TabRelation rel,
    const std::string& url,
    const std::string& title = ""
);

TabData* get_tab_data (int64 id);
int64 get_prev_unclosed_tab (int64 id);  // Returns 0 if there is none.
int64 get_next_unclosed_tab (int64 id);
std::vector<int64> get_all_children (int64 parent);
std::vector<int64> get_all_unclosed_children (int64 parent);
std::vector<int64> get_last_visited_tabs (int n_tabs);
void set_tab_url (int64 id, const std::string& url);
void set_tab_title (int64 id, const std::string& title);
void set_tab_favicon (int64 id, const std::string& favicon);
void set_tab_visited (int64 id);
void star_tab (int64 id);
void unstar_tab (int64 id);
int64 get_last_closed_tab ();
void close_tab (int64 id);
void close_tab_with_heritage (int64 id);
void unclose_tab (int64 id);
void delete_tab_and_children (int64 id);
 // Will prune tabs that are more than more_than *and* older than older_than.
 // (Will always keep at least more_than tabs and all tabs younger than older_than).
void prune_closed_tabs (int64 more_than, double older_than);
void move_tab (int64 id, int64 parent, const Bifractor& position);
void move_tab (int64 id, int64 reference, TabRelation rel);
std::pair<int64, Bifractor> make_location (int64 reference, TabRelation rel);

///// WINDOWS

struct WindowData {
    int64 id;
    int64 root_tab;
    int64 focused_tab;
    double created_at;
    double closed_at;
    WindowData(
        int64 id,
        int64 root_tab,
        int64 focused_tab,
        double created_at,
        double closed_at
    ) :
        id(id),
        root_tab(root_tab),
        focused_tab(focused_tab),
        created_at(created_at),
        closed_at(closed_at)
    { }
};

int64 create_window (int64 root_tab, int64 focused_tab);
WindowData* get_window_data (int64 id);
std::vector<int64> get_all_unclosed_windows ();
int64 get_last_closed_window ();
void set_window_root_tab (int64 window, int64 tab);
void set_window_focused_tab (int64 window, int64 tab);
void close_window (int64 id);
void unclose_window (int64 id);

///// TRANSACTIONS

struct Transaction {
    Transaction ();
    ~Transaction ();
};

struct Observer {
    virtual void Observer_after_commit (
        const std::vector<int64>& updated_tabs,
        const std::vector<int64>& updated_windows
    ) = 0;
    Observer();
    ~Observer();
};

 // Don't do anything but mark the item as updated
void tab_updated (int64);
void window_updated (int64);

///// MISC

void fix_problems ();


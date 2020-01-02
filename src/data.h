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
    double created_at;
    double visited_at;
    double closed_at;
    TabData(
        int64 parent,
        const Bifractor& position,
        int64 child_count,
        const std::string& url,
        const std::string& title,
        double created_at,
        double visited_at,
        double closed_at
    ) :
        parent(parent),
        position(position),
        child_count(child_count),
        url(url),
        title(title),
        created_at(created_at),
        visited_at(visited_at),
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
std::vector<int64> get_all_unclosed_children (int64 parent);
void set_tab_url (int64 id, const std::string& url);
void set_tab_title (int64 id, const std::string& title);
void close_tab (int64 id);
void move_tab (int64 id, int64 parent, const Bifractor& position);
void move_tab (int64 id, int64 reference, TabRelation rel);
std::pair<int64, Bifractor> make_location (int64 reference, TabRelation rel);

///// WINDOWS

struct WindowData {
    int64 id;
    int64 focused_tab;
    double created_at;
    double closed_at;
    WindowData(
        int64 id,
        int64 focused_tab,
        double created_at,
        double closed_at
    ) :
        id(id),
        focused_tab(focused_tab),
        created_at(created_at),
        closed_at(closed_at)
    { }
};

int64 create_window (int64 focused_tab);
std::vector<WindowData> get_all_unclosed_windows ();
void set_window_focused_tab (int64 window, int64 tab);
int64 get_window_focused_tab (int64 window);

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

///// MISC

void fix_problems ();


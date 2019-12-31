#pragma once

#include <string>
#include <vector>

#include "stuff.h"

///// TABS

enum TabType {
    WEBPAGE
};

enum class TabRelation {
    BEFORE,
    AFTER,
    FIRST_CHILD,
    LAST_CHILD
};

struct TabData {
    int64 parent;
    int64 prev;
    int64 next;
    int64 child_count;
    uint8 tab_type;
    std::string url;
    std::string title;
    double created_at;
    double visited_at;
    double closed_at;
    TabData(
        int64 parent,
        int64 prev,
        int64 next,
        int64 child_count,
        uint8 tab_type,
        const std::string& url,
        const std::string& title,
        double created_at,
        double visited_at,
        double closed_at
    ) :
        parent(parent),
        prev(prev),
        next(next),
        child_count(child_count),
        tab_type(tab_type),
        url(url),
        title(title),
        created_at(created_at),
        visited_at(visited_at),
        closed_at(closed_at)
    { }
};

int64 create_webpage_tab (
    int64 reference,
    TabRelation rel,
    const std::string& url,
    const std::string& title = ""
);

TabData* get_tab_data (int64 id);
std::vector<int64> get_all_children (int64 parent);
void set_tab_url(int64 id, const std::string& url);
void set_tab_title(int64 id, const std::string& title);
void close_tab(int64 id);
void move_tab(int64 id, int64 reference, TabRelation rel);

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

void init_db ();
void fix_problems ();


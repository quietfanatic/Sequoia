#pragma once

#include <string>
#include <vector>

#include "stuff.h"

enum Tab_Type {
    WEBPAGE
};

 // Creates tab as the last child of the parent
int64 create_webpage_tab (int64 parent, const std::string& url, const std::string& title = "");

struct TabData {
    int64 parent;
    int64 prev;
    int64 next;
    uint child_count;
    uint8 tab_type;
    std::string url;
    std::string title;
    double created_at;
    double visited_at;
    double trashed_at;
    TabData(
        int64 parent,
        int64 prev,
        int64 next,
        uint child_count,
        uint8 tab_type,
        const std::string& url,
        const std::string& title,
        double created_at,
        double visited_at,
        double trashed_at
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
        trashed_at(trashed_at)
    { }
};

TabData get_tab_data (int64);

std::string get_tab_url (int64 id);
void set_tab_url(int64 id, const std::string& url);

void set_tab_title(int64 id, const std::string& title);

void trash_tab(int64 id);

struct Transaction {
    Transaction ();
    ~Transaction ();
};

struct Observer {
    virtual void Observer_after_commit (
        const std::vector<int64>& updated_tabs
    ) = 0;
    Observer();
    ~Observer();
};


#pragma once

#include <string>
#include <vector>

#include "stuff.h"

struct Activity;

struct Tab {
    int64 id;
    int64 parent;
    int64 next;
    int64 prev;
    uint32 child_count;
    uint8 tab_type;
    uint64 url_hash;
    std::string url;
    std::string title;
    double created_at;
    double last_loaded_at;

    Activity* activity = nullptr;
    Tab* old_data = nullptr;

    enum Type {
        WEBPAGE
    };

    enum SpecialParent {
        ROOT = 0,
        TRASH = -1,
        DELETING = -9
    };

    static Tab* by_id (int64 id);

    static Tab* new_webpage_tab (
        int64 parent,
        const std::string& url,
        const std::string& title = ""
    );

    void set_url (const std::string& u);
    void set_title (const std::string& t);

    void load ();

    Tab* to_focus_on_close ();
    void close ();

    static void commit ();

    void update ();

    ~Tab();
};

struct TabObserver {
    virtual void TabObserver_on_commit (const std::vector<Tab*>& updated_tabs) = 0;
    TabObserver();
    ~TabObserver();
};

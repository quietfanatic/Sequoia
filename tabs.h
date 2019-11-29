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
    std::wstring url;
    std::wstring title;
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

    static Tab* open_webpage (
        int64 parent,
        const std::wstring& url,
        const std::wstring& title = L""
    );

    void set_url (const std::wstring& u);
    void set_title (const std::wstring& t);

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

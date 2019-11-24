#pragma once

#include <map>
#include <string>

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

    enum Type {
        WEBPAGE
    };

    static Tab* open_webpage (
        int64 parent,
        const std::wstring& url,
        const std::wstring& title = L""
    );
    void set_url (const std::wstring& u);
    void set_title (const std::wstring& t);
    void close ();
};

extern std::map<int64, Tab*> tabs_by_id;

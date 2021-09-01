#pragma once

#include <string>
#include <vector>

#include "util/assert.h"
#include "util/bifractor.h"
#include "util/types.h"

template <class T>
struct IDHandle {
    int64 id;

    explicit IDHandle (int64 id = 0) : id(id) { }
    IDHandle (const T& data) : id(data.id) { AA(id); }
    operator int64 () const { return id; }

    T load () const { T data; data.id = *this; data.load(); return data; }
};

///// PAGES

 // I don't know who defined DELETE as a macro but I'm mad
enum class Method : int8 {
    Get,
    Post,
    Put,
    Delete,
    Unknown = -1
};

struct PageData;
using PageID = IDHandle<PageData>;
struct PageData {
    PageID id;
    std::string url;
    Method method = Method::Get;
    int64 group = 0; // NYI
    std::string favicon_url;
    double visited_at = 0;
    std::string title;
    bool exists = true;

    void load ();  // Populate *this with row from database
    void save ();  // Write *this to database
    void updated ();  // Send to Observers without saving
};

std::vector<PageID> get_pages_with_url (const std::string& url);

///// LINKS

struct LinkData;
using LinkID = IDHandle<LinkData>;
struct LinkData {
    LinkID id;
    PageID opener_page;
    PageID from_page;
    PageID to_page;
    Bifractor position;
    std::string title = 0;
    double trashed_at = 0;
    double created_at = 0;
    bool exists = true;

    void load ();
    void save ();
    void updated ();

     // These modify *this but do not save it
    void move_before (LinkID next_link);
    void move_after (LinkID prev_link);
    void move_first_child (PageID from_page);
    void move_last_child (PageID from_page);
};

std::vector<LinkID> get_links_from_page (PageID page);
std::vector<LinkID> get_links_to_page (PageID page);
LinkID get_last_trashed_link ();

///// TAGS

// TODO

///// GROUPS

// TODO

///// VIEWS

struct ViewData;
using ViewID = IDHandle<ViewData>;
struct ViewData {
    ViewID id;
    PageID root_page;
    LinkID focused_link;
    double closed_at = 0;
    double trashed_at = 0;
    bool exists = true;

    void load ();
    void save ();
    void updated ();
};

std::vector<ViewID> get_open_views ();
 // Returns 0 if none
ViewID get_last_closed_view ();

///// VIEWLINKS

struct ViewLinkData {
    ViewID view;
    LinkID link;
    bool exists = true;

    void load ();
    void save ();
    void updated ();
};

std::vector<LinkID> get_view_link_links_with_view (ViewID);

///// TRANSACTIONS

struct Transaction {
    Transaction ();
    ~Transaction ();
};

struct Update {
    std::vector<PageID> pages;
    std::vector<LinkID> links;
    std::vector<ViewID> views;
    std::vector<std::pair<ViewID, LinkID>> view_links;
};

struct Observer {
    virtual void Observer_after_commit (const Update&) = 0;
    Observer();
    ~Observer();
};

///// MISC

namespace std {
    template <class T>
    struct hash<IDHandle<T>> {
        std::size_t operator () (IDHandle<T> v) const { return std::hash<int64>{}(v.id); }
    };
}


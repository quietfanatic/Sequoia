#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include "../util/assert.h"
#include "../util/bifractor.h"
#include "../util/types.h"

namespace model {

template <class T>
struct IDHandle {
    int64 id;

    explicit constexpr IDHandle (int64 id = 0) : id(id) { }
    IDHandle (const T& data) : id(data.id) { AA(id); }
    constexpr operator int64 () const { return id; }

     // If you want to modify the data, you have to copy it first.
     //  The address of the return is not guaranteed to be stable, so don't
     //  keep it around anywhere.
    const T& operator * () const {
        AA(id);
        static int64 last_id = 0;
        static const T* last_data;
        if (last_id != id) {
            last_id = id;
            last_data = T::load(*this);
        }
        return *last_data;
    }
    const T* operator -> () const { return &**this; }
};

} namespace std {
    template <class T>
    struct hash<model::IDHandle<T>> {
        std::size_t operator () (model::IDHandle<T> v) const {
            return std::hash<int64>{}(v.id);
        }
    };
} namespace model {

///// PAGES

 // I don't know who defined DELETE as a macro but I'm mad
enum class Method : int8 {
    Unknown,
    Get,
    Post,
    Put,
    Delete,
};

struct Page;
using PageID = IDHandle<Page>;
struct Page {
    PageID id;
    std::string url;
    Method method = Method::Get;
    int64 group = 0; // NYI
    std::string favicon_url;
    double visited_at = 0;
    std::string title;
    bool exists = true;

    static const Page* load (PageID);
    void save ();  // Write *this to database
    void updated ();  // Send to Observers without saving
};

std::vector<PageID> get_pages_with_url (const std::string& url);

///// LINKS

struct Link;
using LinkID = IDHandle<Link>;
struct Link {
    LinkID id;
    PageID opener_page;
    PageID from_page;
    PageID to_page;
    Bifractor position;
    std::string title;
    double created_at = 0;
    double trashed_at = 0;
    bool exists = true;

    static const Link* load (LinkID);
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

struct View;
using ViewID = IDHandle<View>;
struct View {
    ViewID id;
    PageID root_page;
    LinkID focused_tab;
    double closed_at = 0;
    double created_at = 0;
    double trashed_at = 0;
    bool exists = true;
    std::unordered_set<LinkID> expanded_tabs;

    PageID focused_page () { return focused_tab ? focused_tab->to_page : root_page; }

    static const View* load (ViewID);
    void save ();
    void updated ();
};

std::vector<ViewID> get_open_views ();
 // Returns 0 if none
ViewID get_last_closed_view ();

///// TRANSACTIONS

struct Transaction {
    Transaction ();
    ~Transaction ();
};

struct Update {
    std::unordered_set<PageID> pages;
    std::unordered_set<LinkID> links;
    std::unordered_set<ViewID> views;
};

struct Observer {
    virtual void Observer_after_commit (const Update&) = 0;
    Observer();
    ~Observer();
};

} // namespace model

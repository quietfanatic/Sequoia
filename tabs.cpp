#include "tabs.h"

#include <chrono>
#include <map>

#include "window.h"

static std::map<int64, Tab*> tabs_by_id;
static std::vector<Tab*> updating_tabs;
static std::vector<TabObserver*> all_observers;
static bool committing = false;
static bool commit_again = false;

double now () {
    using namespace std::chrono;
    return duration<double>(system_clock::now().time_since_epoch()).count();
}


/*static*/ Tab* Tab::by_id (int64 id) {
    auto iter = tabs_by_id.find(id);
    if (iter != tabs_by_id.end()) return iter->second;
    else return nullptr;
}

static int64 next_id = 1;
/*static*/ Tab* Tab::open_webpage (int64 parent, const std::wstring& url, const std::wstring& title) {
    Tab* tab = new Tab{next_id++, parent, 0, 0, 0, WEBPAGE, 0, url, title, now(), 0};
    tabs_by_id.emplace(tab->id, tab);
    tab->update();
    return tab;
}

void Tab::set_url (const std::wstring& u) {
    update();
    url = u;
}

void Tab::set_title (const std::wstring& t) {
    update();
    title = t;
}

Tab* Tab::to_focus_on_close () {
    Tab* data = old_data ? old_data : this;
    int64 successor = data->next ? data->next
                    : data->parent ? data->parent
                    : data->prev;
    if (Tab* t = by_id(successor)) {
        if (t->parent != DELETING) return t;
        else return t->to_focus_on_close();
    }
    else return nullptr;
}

void Tab::close () {
    update();
    parent = DELETING;
}

/*static*/ void Tab::commit () {
    if (committing) {
        commit_again = true;
        return;
    }
    committing = true;
    auto updating = std::move(updating_tabs);
    for (auto o : all_observers) {
        o->TabObserver_on_commit(updating);
    }
    for (auto t : updating) {
        delete t->old_data;
        t->old_data = nullptr;
        if (t->parent == DELETING) {
            tabs_by_id.erase(t->id);
            delete t;
        }
    }
    committing = false;
    if (commit_again) {
        commit_again = false;
        commit();
    }
}

void Tab::update () {
    if (!old_data) {
        old_data = new Tab(*this);
        updating_tabs.push_back(this);
    }
}

Tab::~Tab () {
    if (old_data) delete old_data;
}

TabObserver::TabObserver () {
    all_observers.push_back(this);
}

TabObserver::~TabObserver () {
    for (auto i = all_observers.begin(); i != all_observers.end(); i++) {
        if (*i == this) {
            all_observers.erase(i);
            break;
        }
    }
}

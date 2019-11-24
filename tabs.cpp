#include "tabs.h"

#include <chrono>

#include "window.h"

double now () {
    using namespace std::chrono;
    return duration<double>(system_clock::now().time_since_epoch()).count();
}


static int64 next_id = 1;
Tab* Tab::open_webpage (int64 parent, const std::wstring& url, const std::wstring& title) {
    Tab* tab = new Tab{next_id++, parent, 0, 0, 0, WEBPAGE, 0, url, title, now(), 0};
    tabs_by_id.emplace(tab->id, tab);
    return tab;
}

void Tab::set_url (const std::wstring& u) {
    url = u;
    for (auto& w : all_windows) {
        w->update_tab(this);
    }
}

void Tab::set_title (const std::wstring& t) {
    title = t;
    for (auto& w : all_windows) {
        w->update_tab(this);
    }
}

void Tab::close () {
    tabs_by_id.erase(id);
}

std::map<int64, Tab*> tabs_by_id;

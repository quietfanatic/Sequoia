#include "app.h"

#include <unordered_set>
#include <windows.h>

#include "../model/data.h"
#include "../model/data_init.h"
#include "../util/error.h"
#include "activities.h"
#include "bark.h"

#ifndef TAP_DISABLE_TESTS
#include "../tap/tap.h"
#endif

using namespace std;

namespace win32app {

App::App (Profile&& p) :
    profile(std::move(p)),
    settings(profile.load_settings()),
    nursery(*this)
{
    init_db(profile.db_path());
}
App::~App () { }

void App::start (const std::vector<String>& urls) {
    vector<int64> all_windows = get_all_unclosed_windows();
    if (!all_windows.empty()) {
        for (auto id : all_windows) {
             // Create directly instead of going through WindowObserver,
             //  so that focused tabs are not loaded
            barks.emplace(id, new Bark(*this, id));
        }
    }
    else if (int64 w = get_last_closed_window()) {
         // TODO: make this not load the current focused tab?
        unclose_window(w);
    }
    else {
         // Otherwise create a new window if none exists
        int64 first_tab = 0;
        Transaction tr;
        vector<int64> top_level_tabs = get_all_children(0);
        for (auto tab : top_level_tabs) {
            auto data = get_tab_data(tab);
            if (!data->closed_at) {
                first_tab = tab;
                break;
            }
        }
        if (!first_tab) {
            first_tab = create_tab(0, TabRelation::LAST_CHILD, "https://duckduckgo.com/");
        }
        create_window(0, first_tab);
    }
    if (!urls.empty()) {
         // TODO: multiple URLs
        AA(urls.size() == 1);
        auto tab = create_tab(0, TabRelation::LAST_CHILD, urls[0]);
        create_window(0, tab);
    }
}

int App::run () {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessage(GetAncestor(msg.hwnd, GA_ROOT), &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return (int)msg.wParam;
}

void App::quit (int code) {
    PostQuitMessage(code);
}

Activity* App::activity_for_tab (int64 id) {
    auto iter = activities.find(id);
    if (iter == activities.end()) return nullptr;
    else return iter->second.get();
}

Activity* App::ensure_activity_for_tab (int64 id) {
    auto iter = activities.find(id);
    if (iter == activities.end()) {
        iter = activities.emplace(id, new Activity(*this, id)).first;
         // Delete old activities
         // TODO: configurable values
        while (activities.size() > 80) {
            unordered_set<int64> keep_loaded;
             // Don't unload self!
            keep_loaded.emplace(id);
             // Don't unload tabs focused by any windows
            for (auto w : get_all_unclosed_windows()) {
                keep_loaded.emplace(get_window_data(w)->focused_tab);
            }
             // Keep last n loaded tabs regardless
            for (auto t : get_last_visited_tabs(20)) {
                keep_loaded.emplace(t);
            }
             // Find last unstarred tab or last starred tab
            int64 victim_id = 0;
            TabData* victim_dat = nullptr;
            for (auto& [id, activity] : activities) {
                if (keep_loaded.contains(id)) continue;
                auto dat = get_tab_data(id);
                 // Not yet visited?  Not quite sure why this would happen.
                if (dat->visited_at == 0) continue;
                if (!victim_id
                    || !dat->starred_at && victim_dat->starred_at
                    || dat->visited_at < victim_dat->visited_at
                ) {
                    victim_id = id;
                    victim_dat = dat;
                }
            }
            delete_activity(victim_id);
        }
    }
    return iter->second.get();
}

void App::delete_activity (int64 id) {
    activities.erase(id);
}

void App::Observer_after_commit (
    const vector<int64>& updated_tabs,
    const vector<int64>& updated_windows
) {
    for (auto& id : updated_tabs) {
        auto data = get_tab_data(id);
        if (data->closed_at || data->deleted) {
            delete_activity(id);
        }
    }
    for (int64 id : updated_windows) {
        auto data = get_window_data(id);
        auto iter = barks.find(id);
        if (iter == barks.end()) {
            if (!data->closed_at) {
                auto [iter, emplaced] = barks.emplace(id, new Bark(*this, id));
                 // Automatically load focused tab
                iter->second->claim_activity(ensure_activity_for_tab(data->focused_tab));
            }
        }
        else if (data->closed_at) {
            barks.erase(id);
        }
    }
    for (auto& [id, bark] : barks) {
        bark->update(updated_tabs, updated_windows);
    }
    if (barks.empty()) quit();
}

#ifndef TAP_DISABLE_TESTS
static void app_tests () {
    using namespace tap;
    ProfileTestEnvironment env;

    App* app = new App(std::move(env.profile));
    app->headless = true;

    doesnt_throw([&]{
        app->start({});
    }, "Start app (no urls)");

    doesnt_throw([&]{
         // Doing this in this order should make run quit immediately.
        app->quit();
        app->run();
    }, "Run and quit app");

    doesnt_throw([&]{
        delete app;
    }, "Delete app");

    done_testing();
}
static tap::TestSet tests ("win32app/app", &app_tests);
#endif

} // namespace win32app


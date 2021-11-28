#include "app.h"

#include <exception>
#include <windows.h>

#include "../model/database.h"
#include "../model/transaction.h"
#include "activity.h"
#include "shell.h"
#include "window.h"

using namespace std;

App::App (Profile&& p) :
    profile(std::move(p)),
    settings(profile.load_settings())
{
    model::init_db(profile.db_path());
}
App::~App () = default;

int App::run (const vector<String>& urls) {
     // Start browser
    vector<model::ViewID> views = model::get_open_views();
    if (!views.empty()) {
        model::Transaction tr;
        for (model::ViewID view : views) {
            updated(view);
        }
    }
    else if (!urls.empty()) {
         // Open urls later
    }
    else if (model::ViewID view = model::get_last_closed_view()) {
         // TODO: unclose multiple views if they were closed in
         // quick succession
        unclose(view);
    }
    else {
        model::create_view_and_page("about:blank"sv);
    }
     // Open new window if requested
    if (!urls.empty()) {
        if (urls.size() > 1) throw Error("Multiple URL arguments NYI"sv);
        model::create_view_and_page(urls[0]);
    }
     // Run message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessage(GetAncestor(msg.hwnd, GA_ROOT), &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return (int)msg.wParam;
}
void App::quit () {
    PostQuitMessage(0);
}

Window* App::window_for_view (model::ViewID view) {
    auto iter = windows.find(view);
    if (iter != windows.end()) return iter->second.get();
    else return nullptr;
}

Window* App::window_for_page (model::PageID page) {
    if (auto view = load(page)->viewing_view) {
        Window* window = window_for_view(view);
        AA(window);
        return window;
    }
    else return nullptr;
}

Shell* App::shell_for_view (model::ViewID view) {
    auto iter = shells.find(view);
    if (iter != shells.end()) return iter->second.get();
    else return nullptr;
}

Activity* App::activity_for_page (model::PageID page) {
    auto iter = activities.find(page);
    if (iter != activities.end()) return iter->second.get();
    else return nullptr;
}

void App::Observer_after_commit (const model::Update& update) {
     // Create and destroy app objects
    for (model::ViewID view : update.views) {
        auto view_data = load(view);
        if (view_data && !view_data->closed_at) {
            auto& shell = shells[view];
            if (!shell) shell = make_unique<Shell>(this, view);
            auto& window = windows[view];
            if (!window) window = make_unique<Window>(this, view);
        }
        else {
            shells.erase(view);
            windows.erase(view);
        }
    }
    for (model::PageID page : update.pages) {
        auto page_data = load(page);
        if (page_data && page_data->state != model::PageState::UNLOADED) {
            auto& activity = activities[page];
            if (!activity) activity = make_unique<Activity>(this, page);
        }
        else activities.erase(page);
    }
     // Update objects (order shouldn't matter here)
    for (model::ViewID view : update.views) {
        if (Window* window = window_for_view(view)) {
            window->view_updated();
        }
    }
    for (model::PageID page : update.pages) {
        if (Activity* activity = activity_for_page(page)) {
            activity->page_updated();
        }
        if (Window* window = window_for_page(page)) {
            window->page_updated();
        }
    }
    for (auto& [_, shell] : shells) {
        shell->update(update);
    }
};

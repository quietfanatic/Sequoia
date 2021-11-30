#include "app.h"

#include <exception>
#include <windows.h>

#include "../model/page.h"
#include "../model/view.h"
#include "../model/write.h"
#include "activity.h"
#include "shell.h"
#include "window.h"

using namespace std;

namespace win32app {

App::App (Profile&& p) :
    profile(std::move(p)),
    settings(profile.load_settings()),
    nursery(*this),
    model(model::new_model(profile.db_path()))
{
    observe(model, this);
}
App::~App () {
    unobserve(model, this);
    model::delete_model(model);
}

int App::run (const vector<String>& urls) {
     // Start browser
    auto views = get_open_views(model);
    if (!views.empty()) {
        auto transaction = write(model);
        for (auto view : views) touch(transaction, view);
    }
    else if (!urls.empty()) {
         // Open urls later
    }
    else if (auto view = get_last_closed_view(model)) {
         // TODO: unclose multiple views if they were closed in
         // quick succession
        unclose(write(model), view);
    }
    else {
        create_view_and_page(write(model), "about:blank"sv);
    }
     // Open new window if requested
    if (!urls.empty()) {
        if (urls.size() > 1) throw Error("Multiple URL arguments NYI"sv);
        create_view_and_page(write(model), urls[0]);
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
    if (auto view = (model/page)->viewing_view) {
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
     // Create and destroy app objects.  Ideally order shouldn't matter, but I
     // suspect there may be dependencies.
    for (model::ViewID view : update.views) {
        auto view_data = model/view;
        if (view_data && !view_data->closed_at) {
            auto& shell = shells[view];
            if (!shell) shell = make_unique<Shell>(*this, view);
            auto& window = windows[view];
            if (!window) window = make_unique<Window>(*this, view);
        }
        else {
            shells.erase(view);
            windows.erase(view);
        }
    }
    for (model::PageID page : update.pages) {
        auto page_data = model/page;
        if (page_data && page_data->state != model::PageState::UNLOADED) {
            auto& activity = activities[page];
            if (!activity) activity = make_unique<Activity>(*this, page);
        }
        else activities.erase(page);
    }
     // Quit if there are no more windows.
    if (windows.empty()) {
        AA(shells.empty());
        quit();
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
             // TODO: avoid calling this twice?
            window->page_updated();
        }
    }
     // Shells are responsible for figuring out when they want to update.
    for (auto& [_, shell] : shells) {
        shell->update(update);
    }
};

} // namespace win32app

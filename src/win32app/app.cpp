#include "app.h"

#include <exception>
#include <windows.h>

#include "../model/node.h"
#include "../model/view.h"
#include "../model/write.h"
#include "../util/error.h"
#include "activity.h"
#include "shell.h"
#include "window.h"

using namespace std;

namespace win32app {

App::App (Profile&& p) :
    profile(std::move(p)),
    settings(profile.load_settings()),
    nursery(*this),
    model(*model::new_model(profile.db_path()))
{
    observe(model, this);
}

App::~App () {
    unobserve(model, this);
    model::delete_model(&model);
}

Activity* App::activity_for_id (model::ActivityID id) {
    AA(id);
    auto iter = activities.find(id);
    AA(iter != activities.end());
    return &*iter->second;
}

Activity* App::activity_for_view (model::ViewID view) {
    auto id = get_activity_for_view(model, view);
    if (!id) return nullptr;
    else return activity_for_id(id);
}

Shell* App::shell_for_view (model::ViewID view) {
    auto iter = app_views.find(view);
    if (iter == app_views.end()) return nullptr;
    else return &*iter->second.shell;
}

Window* App::window_for_view (model::ViewID view) {
    auto iter = app_views.find(view);
    if (iter == app_views.end()) return nullptr;
    else return &*iter->second.window;
}

void App::open_urls (const vector<String>& urls) {
    AA(!urls.empty());
    if (urls.size() > 1) ERR("Multiple URL arguments NYI"sv);
    auto w = write(model);
    auto view = create_view(w);
    auto node = ensure_node_with_url(w, urls[0]);
    make_last_child(w, (w/view)->root_node, node);
}

void App::start (const vector<String>& urls) {
     // Start browser
    auto open_views = get_open_views(model);
    for (auto view : open_views) {
        app_views.emplace(view, AppView{
            std::make_unique<Shell>(*this, view),
            std::make_unique<Window>(*this, view)
        });
    }
    if (!urls.empty()) {
         // Open new window if requested
        open_urls(urls);
    }
    else if (open_views.empty()) {
         // If no URLs were given and there aren't any open views, make sure at
         // least one window appears.
        if (auto closed_view = get_last_closed_view(model)) {
             // TODO: unclose multiple views if they were closed in
             // quick succession
            unclose(write(model), closed_view);
        }
        else {
             // Create default window
            auto w = write(model);
            auto view = create_view(w);
            make_last_child(w, (w/view)->root_node, model::NodeID{});
        }
    }
}

int App::run () {
#ifndef NDEBUG
    AA(get_open_views(model).size() > 0);
#endif
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

void App::Observer_after_commit (const model::Update& update) {
     // Create, update, destroy activities
    for (auto activity_id : update.activities) {
        auto activity_data = model/activity_id;
        if (activity_data) {
            auto& activity = activities[activity_id];
            if (activity) activity->update();
            else activity = std::make_unique<Activity>(*this, activity_id);
        }
        else activities.erase(activity_id);
    }
     // Update existing shells and windows that aren't going away
    for (auto& [view_id, app_view] : app_views) {
        auto view_data = model/view_id;
        if (view_data) {
             // Send update to all shells and windows, let them decide what they
             // care about.
            app_view.shell->update(update);
            app_view.window->update(update);
        }
    }
     // Create and destroy app views
    for (auto view_id : update.views) {
        auto view_data = model/view_id;
        if (view_data) {
            auto& app_view = app_views[view_id];
            AA(!!app_view.shell == !!app_view.window);
            if (!app_view.shell) {
                app_view.shell = std::make_unique<Shell>(*this, view_id);
            }
            if (!app_view.window) {
                app_view.window = std::make_unique<Window>(*this, view_id);
            }
        }
        else app_views.erase(view_id);
    }
     // Quit if there are no more windows.
    if (app_views.size() == 0) {
        quit();
    }
};

} // namespace win32app

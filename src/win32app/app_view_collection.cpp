#include "app_view_collection.h"

#include "../model/model.h"
#include "../model/observer.h"
#include "../model/view.h"
#include "app.h"
#include "shell.h"
#include "window.h"

namespace win32app {

size_t AppViewCollection::count () {
    return by_view.size();
}
AppView* AppViewCollection::get_for_view (model::ViewID view) {
    auto iter = by_view.find(view);
    if (iter == by_view.end()) return nullptr;
    else return &iter->second;
}

void AppViewCollection::initialize (model::ReadRef model) {
    AA(by_view.empty());
    for (auto view : get_open_views(model)) {
        by_view.emplace(view, AppView{
            std::make_unique<Shell>(app, view),
            std::make_unique<Window>(app, view)
        });
    }
}

void AppViewCollection::update (const model::Update& update) {
     // Update existing views
    for (auto& [view, app_view] : by_view) {
        auto view_data = app.model/view;
        if (view_data && !view_data->closed_at) {
            if (update.views.count(view)) {
                app_view.window->view_updated();
            }
             // Always send update to shell, because it cares about
             // nodes and edges, not just views.
            app_view.shell->update(update);
        }
        else {
             // Don't bother updating doomed app views
        }
    }
     // TODO: update existing windows per node
     // Create and delete
    for (auto view : update.views) {
        auto view_data = app.model/view;
        if (view_data && !view_data->closed_at) {
            auto& app_view = by_view[view];
            AA(!!app_view.shell == !!app_view.window);
            if (!app_view.shell) {
                app_view.shell = std::make_unique<Shell>(app, view);
            }
            if (!app_view.window) {
                app_view.window = std::make_unique<Window>(app, view);
            }
        }
        else {
            by_view.erase(view);
        }
    }
}

} // namespace win32app

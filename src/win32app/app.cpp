#include "app.h"

#include <exception>
#include <windows.h>

#include "../model/node.h"
#include "../model/tree.h"
#include "../model/write.h"
#include "../util/error.h"
#include "activity_view.h"
#include "bark_view.h"
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

ActivityView* App::activity_for_id (model::ActivityID id) {
    AA(id);
    auto iter = activities.find(id);
    AA(iter != activities.end());
    AA(iter->second);
    return &*iter->second;
}

ActivityView* App::activity_for_tree (model::TreeID tree) {
    auto id = get_activity_for_tree(model, tree);
    if (!id) return nullptr;
    else return activity_for_id(id);
}

BarkView* App::bark_for_tree (model::TreeID tree) {
    auto iter = tree_views.find(tree);
    if (iter == tree_views.end()) return nullptr;
    else return &*iter->second.bark;
}

Window* App::window_for_tree (model::TreeID tree) {
    auto iter = tree_views.find(tree);
    if (iter == tree_views.end()) return nullptr;
    else return &*iter->second.window;
}

void App::start (const vector<String>& urls) {
     // Show already open trees
    auto open_trees = get_open_trees(model);
    for (auto tree : open_trees) {
        tree_views.emplace(tree, TreeView{
            std::make_unique<BarkView>(*this, tree),
            std::make_unique<Window>(*this, tree)
        });
    }
    if (!urls.empty()) {
         // Open new window if requested
        open_tree_for_urls(write(model), urls);
    }
    else if (open_trees.empty()) {
        auto w = write(model);
         // If no URLs were given and there aren't any open trees, make sure at
         // least one window appears.
        unclose_recently_closed_trees(w);
        if (get_open_trees(w).empty()) {
            create_default_tree(w);
        }
    }
}

int App::run () {
#ifndef NDEBUG
    AA(get_open_trees(model).size() > 0);
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

void App::async (std::function<void()>&& f) {
    nursery.async(std::move(f));
}

void App::Observer_after_commit (const model::Update& update) {
     // Create, update, destroy activities
    for (auto activity_id : update.activities) {
        auto activity_data = model/activity_id;
        if (activity_data) {
            auto& activity = activities[activity_id];
            if (activity) activity->update();
            else activity = std::make_unique<ActivityView>(*this, activity_id);
        }
        else activities.erase(activity_id);
    }
     // Update existing barks and windows that aren't going away
    for (auto& [tree_id, tree_view] : tree_views) {
        auto tree_data = model/tree_id;
        if (tree_data && !tree_data->closed_at) {
             // Send update to all barks and windows, let them decide what they
             // care about.
            tree_view.bark->update(update);
            tree_view.window->update(update);
        }
    }
     // Create and destroy app trees
    for (auto tree_id : update.trees) {
        auto tree_data = model/tree_id;
        if (tree_data && !tree_data->closed_at) {
            auto& tree_view = tree_views[tree_id];
            AA(!!tree_view.bark == !!tree_view.window);
            if (!tree_view.bark) {
                tree_view.bark = std::make_unique<BarkView>(*this, tree_id);
            }
            if (!tree_view.window) {
                tree_view.window = std::make_unique<Window>(*this, tree_id);
            }
        }
        else tree_views.erase(tree_id);
    }
     // Quit if there are no more windows.
    if (tree_views.size() == 0) {
        quit();
    }
};

} // namespace win32app

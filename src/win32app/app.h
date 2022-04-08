#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "../model/activity.h"
#include "../model/model.h"
#include "../model/observer.h"
#include "profile.h"
#include "nursery.h"

namespace win32app {
struct ActivityView;
struct Bark;
struct Window;

struct TreeView {
    std::unique_ptr<Bark> bark;
    std::unique_ptr<Window> window;
};

struct App : model::Observer {
    Profile profile;
    Settings settings;
    Nursery nursery;
    model::Model& model;
    std::unordered_map<model::ActivityID, std::unique_ptr<ActivityView>> activities;
    std::unordered_map<model::TreeID, TreeView> tree_views;

     // Makes all windows hidden; mainly for testing.
    bool headless = false;

    App (Profile&& profile);
    ~App();

    ActivityView* activity_for_id (model::ActivityID);
    ActivityView* activity_for_tree (model::TreeID);
    Bark* bark_for_tree (model::TreeID);
    Window* window_for_tree (model::TreeID);

     // Make windows for open trees, open window for urls, and if none
     // of those happens, makes a default window.
    void start (const std::vector<String>& urls);

     // Runs a standard win32 message loop until quit() is called.
    int run ();
    void quit (int code = 0);

     // Queues a message in the message loop to run an arbitrary function.
    void async (std::function<void()>&&);

    void Observer_after_commit (const model::Update&) override;
};

} // namespace win32app

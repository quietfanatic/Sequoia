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
struct Activity;
struct Shell;
struct Window;

struct AppView {
    std::unique_ptr<Shell> shell;
    std::unique_ptr<Window> window;
};

struct App : model::Observer {
    Profile profile;
    Settings settings;
    Nursery nursery;
    model::Model& model;
    std::unordered_map<model::ActivityID, std::unique_ptr<Activity>> activities;
    std::unordered_map<model::ViewID, AppView> app_views;

     // Makes all windows hidden; mainly for testing.
    bool headless = false;

    App (Profile&& profile);
    ~App();

    Activity* activity_for_id (model::ActivityID);
    Activity* activity_for_view (model::ViewID);
    Shell* shell_for_view (model::ViewID);
    Window* window_for_view (model::ViewID);

     // Make windows for open views, open window for urls, and if none
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

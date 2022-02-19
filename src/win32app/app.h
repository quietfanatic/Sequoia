#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "../model/model.h"
#include "../model/observer.h"
#include "profile.h"
#include "nursery.h"

namespace win32app {
struct Activity;
struct ActivityCollection;
struct AppViewCollection;
struct Shell;
struct Window;

struct App : model::Observer {
    Profile profile;
    Settings settings;
    Nursery nursery;
    model::Model& model;
    std::unique_ptr<ActivityCollection> activities;
    std::unique_ptr<AppViewCollection> app_views;

     // Makes all windows hidden; mainly for testing.
    bool headless = false;

    App (Profile&& profile);
    ~App();

     // Just shortcuts for *Collection
    Activity* activity_for_node (model::NodeID);
    Activity* ensure_activity_for_node (model::NodeID);
    Shell* shell_for_view (model::ViewID);
    Window* window_for_view (model::ViewID);

     // Opens a new window with these URLs.
    void open_urls (const std::vector<String>& urls);

     // Like run, but doesn't start the message loop.
    void start (const std::vector<String>& urls);
     // Doesn't return until quit is called
    int run (const std::vector<String>& urls);
    void quit (int code = 0);

    void Observer_after_commit (const model::Update&) override;
};

} // namespace win32app

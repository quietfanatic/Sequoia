#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "../model/page.h"
#include "../model/transaction.h"
#include "../model/view.h"
#include "profile.h"

struct Activity;
struct Shell;
struct Window;

struct App : model::Observer {
    Profile profile;
    Settings settings;
    std::unordered_map<model::ViewID, std::unique_ptr<Window>> windows;
    std::unordered_map<model::ViewID, std::unique_ptr<Shell>> shells;
    std::unordered_map<model::PageID, std::unique_ptr<Activity>> activities;

    Window* window_for_view (model::ViewID);
    Window* window_for_page (model::PageID);
    Shell* shell_for_view (model::ViewID);
    Activity* activity_for_page (model::PageID);

    App (Profile&& profile);
    ~App();
     // Doesn't return until quit is called
    int run (const std::vector<String>& urls);
    void quit ();

    void Observer_after_commit (const model::Update&) override;
};

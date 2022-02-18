#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "../model/model.h"
#include "../model/observer.h"
#include "profile.h"
#include "nursery.h"

namespace win32app {
struct ActivityCollection;
struct Activity;
struct Shell;
struct Window;

struct App : model::Observer {
    Profile profile;
    Settings settings;
    Nursery nursery;
    model::Model& model;
    std::unique_ptr<ActivityCollection> activities;
    std::unordered_map<model::ViewID, std::unique_ptr<Shell>> shells;
    std::unordered_map<model::ViewID, std::unique_ptr<Window>> windows;

    Window* window_for_view (model::ViewID);
    Shell* shell_for_view (model::ViewID);

    App (Profile&& profile);
    ~App();
     // Doesn't return until quit is called
    int run (const std::vector<String>& urls);
    void quit ();

    void Observer_after_commit (const model::Update&) override;
};

} // namespace win32app

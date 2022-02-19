#pragma once

#include <memory>
#include <unordered_map>

#include "../model/model.h"

namespace model { struct Update; }

namespace win32app {
struct App;
struct Shell;
struct Window;

struct AppView {
    std::unique_ptr<Shell> shell;
    std::unique_ptr<Window> window;
};

struct AppViewCollection {
    App& app;
    std::unordered_map<model::ViewID, AppView> by_view;

    size_t count ();
    AppView* get_for_view (model::ViewID);

    void initialize (model::ReadRef);
     // Creates and deletes app views as necessary to ensure that there is one
     // per open view in the model.
    void update (const model::Update&);
};

} // namespace win32app

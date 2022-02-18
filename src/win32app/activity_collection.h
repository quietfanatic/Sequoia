#pragma once

#include <memory>
#include <unordered_map>

#include "../model/model.h"

namespace model { struct Update; }

namespace win32app {
struct Activity;
struct App;

struct ActivityCollection {
    App& app;
    std::unordered_map<model::NodeID, std::unique_ptr<Activity>> by_node;

    Activity* get_for_node (model::NodeID);
    Activity* ensure_for_node (model::NodeID);

    void insert (std::unique_ptr<Activity>&&);
    std::unique_ptr<Activity> extract_for_node (model::NodeID);

    void update (const model::Update&);
};

} // namespace win32app

#include "activity_collection.h"

#include "../model/observer.h"
#include "activity.h"
#include "app.h"

using namespace std;

namespace win32app {

Activity* ActivityCollection::get_for_node (model::NodeID node) {
    if (!node) return nullptr;
    auto iter = by_node.find(node);
    if (iter == by_node.end()) return nullptr;
    else return &*iter->second;
}

Activity* ActivityCollection::ensure_for_node (model::NodeID node) {
    AA(node);
    auto& ptr = by_node[node];
    if (!ptr) ptr = std::make_unique<Activity>(app, node);
    return &*ptr;
}

void ActivityCollection::insert (std::unique_ptr<Activity>&& activity) {
    AA(!!activity);
    auto [iter, emplaced] = by_node.emplace(
        activity->node, std::move(activity)
    );
    AA(emplaced);
}

std::unique_ptr<Activity> ActivityCollection::extract_for_node (
    model::NodeID node
) {
    AA(node);
    auto iter = by_node.find(node);
    if (iter == by_node.end()) return nullptr;
    auto ptr = std::move(iter->second);
    by_node.erase(iter);
    return ptr;
}

void ActivityCollection::update (const model::Update& update) {
    for (auto node : update.nodes) {
        if (app.model/node) {
            if (Activity* activity = get_for_node(node)) {
                activity->update();
            }
        }
        else {
            extract_for_node(node);
        }
    }
}

} // namespace win32app

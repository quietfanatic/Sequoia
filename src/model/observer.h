#pragma once

#include <unordered_set>
#include "model.h"

namespace model {

 // Collection of item IDs passed to Observers
struct Update {
    std::unordered_set<NodeID> nodes;
    std::unordered_set<EdgeID> edges;
    std::unordered_set<ViewID> views;
    std::unordered_set<ActivityID> activities;
};

 // Whenever the model is changed (see write.h), all Observers attached to the
 // model will be notified with the IDs of items that were changed.
 // Observer_after_commit doesn't have to be reentrant; you can make a change
 // to the model in Observer_after_commit, and the next update will wait until
 // the current update is done.
struct Observer {
    virtual void Observer_after_commit (const Update&) = 0;
};

 // Attach or detach an observer.  If you do these while observers are being
 // notified, they won't take effect until the next update.
void observe (Model&, Observer*);
void unobserve (Model&, Observer*);

} // namespace model

#pragma once

#include <unordered_set>
#include <vector>

#include "model.h"

namespace model {

 // An Write starts a transaction when constructed, and commits the transaction
 // when destroyed (or rolls it back if it was destroyed due to an exception).
 //
 // Functions that alter the Model take a Write instead of a Model as the first
 // argument.  The Observers attached to the Model will be notified with the
 // IDs of any items that were changed during the Write.  If observers are
 // currently being notified when this happens, the next wave of notifications
 // will wait until the current one is done.
 //
 // Only one Write can be active at a time, though you can start another Write
 // while observers are being notified in a Write's destructor.
struct Write {
    Model& model;

    explicit Write (Model&);
    Write (const Write&) = delete;
    ~Write();
};

 // ADL doesn't work on constructors.
static inline Write write (Model& model) { return Write(model); }

struct WriteRef {
    Model& model;
    WriteRef (Write& w) : model(w.model) { }
    WriteRef (Write&& w) : model(w.model) { }
    operator ReadRef () { return Read(model); }
    Model& operator* () { return model; }
    Model* operator-> () { return &model; }
};

} // namespace model

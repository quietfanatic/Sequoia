#include "../observer.h"

#include "model-internal.h"

using namespace std;

namespace model {

void observe (Model& model, Observer* observer) {
    model.writes.observers.emplace_back(observer);
}
void unobserve (Model& model, Observer* observer) {
    erase(model.writes.observers, observer);
}

} // namespace model

#include "../observer.h"

#include "model-internal.h"

using namespace std;

namespace model {

void observe (Model& model, Observer* observer) {
    model.writes.observers.emplace_back(observer);
}
void unobserve (Model& model, Observer* observer) {
    auto& observers = model.writes.observers;
    for (auto iter = observers.begin(); iter != observers.end(); iter++) {
        if (*iter == observer) {
            observers.erase(iter);
            break;
        }
    }
}

} // namespace model

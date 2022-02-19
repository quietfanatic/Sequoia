#pragma once
#ifndef TAP_DISABLE_TESTS

#include "../../model.h"
#include "../tap/tap.h"

namespace model {

struct ModelTestEnvironment {
    String test_folder;
    String db_path;
    ModelTestEnvironment ();
    ~ModelTestEnvironment ();
};

} // namespace model

namespace tap {
    template <class T>
    struct Show<::model::ModelID<T>> {
        std::string show (::model::ModelID<T> id) {
            return "{" + Show<int64>{}.show(int64(id)) + "}";
        }
    };
}

#endif // TAP_DISABLE_TESTS

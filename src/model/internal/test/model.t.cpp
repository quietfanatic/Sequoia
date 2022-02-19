#ifndef TAP_DISABLE_TESTS
#include "../../model.h"

#include <filesystem>

#include "model_test_environment.h"

namespace model {

void model_tests () {
    using namespace tap;
    ModelTestEnvironment env;

    Model* model;
    doesnt_throw([&]{
        model = new_model(env.db_path);
    }, "new_model can create new DB file");
    doesnt_throw([&]{
        delete_model(model);
    }, "delete_model");
    ok(filesystem::file_size(env.db_path) > 0, "delete_model leaves DB file");
    doesnt_throw([&]{
        model = new_model(env.db_path);
        delete_model(model);
    }, "new_model can use existing DB file");

    done_testing();
}
static tap::TestSet tests ("model/model", model_tests);

} // namespace model

#endif // TAP_DISABLE_TESTS

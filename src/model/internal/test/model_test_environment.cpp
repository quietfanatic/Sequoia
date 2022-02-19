#ifndef TAP_DISABLE_TESTS
#include "model_test_environment.h"

#include <filesystem>
#include <stdexcept>
#include "../../../util/files.h"
#include "../../../util/log.h"

namespace model {
using namespace std;

ModelTestEnvironment::ModelTestEnvironment () {
    test_folder = exe_relative("test"sv);
     // TODO: fix ProfileTestEnvironment so it doesn't hog this folder
    //filesystem::remove_all(test_folder);
    filesystem::create_directories(test_folder);
    init_log(test_folder + "/test.log"s);
    db_path = test_folder + "/test-db.sqlite"s;
}

ModelTestEnvironment::~ModelTestEnvironment () {
     // The runtime on Windows is not very helpful when an exception is thrown
     //  in a destructor.
    if (uncaught_exceptions()) {
        try {
            uninit_log();
//            filesystem::remove_all(test_folder);
        }
        catch (std::exception& e) {
            tap::diag(e.what());
        }
    }
    else {
        uninit_log();
//        filesystem::remove_all(test_folder);
    }
}

} // namespace model

#endif // TAP_DISABLE_TESTS

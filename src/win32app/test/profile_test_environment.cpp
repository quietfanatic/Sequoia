#ifndef TAP_DISABLE_TESTS
#include "profile_test_environment.h"

#include <filesystem>
#include <stdexcept>
#include "../../tap/tap.h"
#include "../../util/files.h"
#include "../../util/log.h"

using namespace std;

namespace win32app {

ProfileTestEnvironment::ProfileTestEnvironment () {
    test_folder = exe_relative("test"sv);
    profile_name = "test-profile"s;
    profile_folder = test_folder + "/"s + profile_name;
    filesystem::remove_all(test_folder);
    filesystem::create_directories(test_folder);
}

ProfileTestEnvironment::~ProfileTestEnvironment () {
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
           // TODO: make this work (wait for browser process to close?)
//        filesystem::remove_all(test_folder);
    }
}

} // namespace win32app

#endif // TAP_DISABLE_TESTS

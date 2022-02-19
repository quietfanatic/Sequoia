#ifndef TAP_DISABLE_TESTS
#include "../profile.h"

#include "../../tap/tap.h"
#include "profile_test_environment.h"

namespace win32app {

void profile_tests () {
    using namespace tap;
    ProfileTestEnvironment* env;

    doesnt_throw([&]{
        env = new ProfileTestEnvironment;
    }, "constructor");
    doesnt_throw([&]{
        env->profile.load_settings();
    }, "load_settings");
    doesnt_throw([&]{
        delete env;
    }, "destructor");

    done_testing();
}
static tap::TestSet tests ("win32app/profile", profile_tests);

} // namespace win32app

#endif

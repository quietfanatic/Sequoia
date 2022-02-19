#pragma once
#ifndef TAP_DISABLE_TESTS

#include "../util/types.h"
#include "../profile.h"

namespace win32app {

struct ProfileTestEnvironment {
    String test_folder;
    String profile_name;
    String profile_folder;
    Profile profile;
    ProfileTestEnvironment();
    ~ProfileTestEnvironment();
};

} // namespace win32app

#endif

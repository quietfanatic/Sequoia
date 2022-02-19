#pragma once
#ifndef TAP_DISABLE_TESTS

#include "../util/types.h"

namespace win32app {

struct ProfileTestEnvironment {
    String test_folder;
    String profile_name;
    String profile_folder;
    ProfileTestEnvironment();
    ~ProfileTestEnvironment();
};

} // namespace win32app

#endif

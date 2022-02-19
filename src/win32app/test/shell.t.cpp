#ifndef TAP_DISABLE_TESTS
#include "../shell.h"

#include "../../model/view.h"
#include "../../tap/tap.h"
#include "../app.h"
#include "../profile.h"
#include "profile_test_environment.h"

namespace win32app {

static void shell_tests () {
    using namespace tap;
    ProfileTestEnvironment env;

    App app (Profile(env.profile_name, env.profile_folder));
    app.headless = true;
    app.start({});
    auto open_views = get_open_views(app.model);
    AA(open_views.size() == 1);
    doesnt_throw([&]{
        app.shell_for_view(open_views[0])->wait_for_ready();
    }, "Shell is ready");
    done_testing();
}
static tap::TestSet tests ("win32app/shell", shell_tests);

} // namespace win32app

#endif

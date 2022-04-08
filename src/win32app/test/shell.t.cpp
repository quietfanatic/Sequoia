#ifndef TAP_DISABLE_TESTS
#include "../shell.h"

#include "../../model/tree.h"
#include "../../tap/tap.h"
#include "../app.h"
#include "../profile.h"
#include "profile_test_environment.h"

namespace win32app {

static void shell_tests () {
    using namespace tap;
    ProfileTestEnvironment env;

    App app (std::move(env.profile));
    app.headless = true;
    app.start({});
    auto open_trees = get_open_trees(app.model);
    AA(open_trees.size() == 1);
    doesnt_throw([&]{
        app.shell_for_tree(open_trees[0])->wait_for_ready();
    }, "Shell is ready");
    done_testing();
}
static tap::TestSet tests ("win32app/shell", shell_tests);

} // namespace win32app

#endif

#ifndef TAP_DISABLE_TESTS
#include "../bark.h"

#include "../../model/tree.h"
#include "../../tap/tap.h"
#include "../app.h"
#include "../profile.h"
#include "profile_test_environment.h"

namespace win32app {

static void bark_tests () {
    using namespace tap;
    ProfileTestEnvironment env;

    App app (std::move(env.profile));
    app.headless = true;
    app.start({});
    auto open_trees = get_open_trees(app.model);
    AA(open_trees.size() == 1);
    doesnt_throw([&]{
        app.bark_for_tree(open_trees[0])->wait_for_ready();
    }, "Bark is ready");
    done_testing();
}
static tap::TestSet tests ("win32app/bark", bark_tests);

} // namespace win32app

#endif

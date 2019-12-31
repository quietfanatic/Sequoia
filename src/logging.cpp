#include "logging.h"

#include <chrono>
#include <fstream>

#include "assert.h"
#include "settings.h"

using namespace std;
using namespace std::chrono;

ostream& logstream () {
    A(!profile_folder.empty());
    static ofstream log (profile_folder + "/Sequoia.log");
    double now = duration<double>(steady_clock::now().time_since_epoch()).count();
    log << now << ": ";
    return log;
}


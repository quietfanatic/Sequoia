#include "logging.h"

#include <chrono>
#include <fstream>

using namespace std;
using namespace std::chrono;

ostream& logstream () {
    static ofstream log ("Sequoia.log");
    double now = duration<double>(steady_clock::now().time_since_epoch()).count();
    log << now << ": ";
    return log;
}


#include "logging.h"

#include <chrono>
#include <fstream>

#include "assert.h"

using namespace std;
using namespace std::chrono;

std::ostream* logstream;

void init_log (const std::string& filename) {
    A(!logstream);
    A(!filename.empty());
    logstream = new ofstream (filename);
}

static double now () {
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

uint64 logging_timestamp () {
    static double start = now();
    return uint64((now() - start) * 1000);
}

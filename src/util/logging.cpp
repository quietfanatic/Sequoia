#include "logging.h"

#include <chrono>
#include <fstream>

#include "assert.h"
#include "time.h"

using namespace std;
using namespace std::chrono;

std::ostream* logstream;

void init_log (const std::string& filename) {
    AA(!logstream);
    AA(!filename.empty());
    logstream = new ofstream (filename);
}

uint64 logging_timestamp () {
    static double start = now();
    return uint64((now() - start) * 1000);
}

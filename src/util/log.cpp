#include "log.h"

#include <chrono>
#include <fstream>

#include "assert.h"
#include "time.h"

using namespace std;
using namespace std::chrono;

std::ostream* logstream;

void init_log (Str filename) {
    AA(!logstream);
    AA(!filename.empty());
    logstream = new ofstream (String(filename));
}

void uninit_log () {
    delete logstream;
    logstream = nullptr;
}

uint64 logging_timestamp () {
    static double start = now();
    return uint64((now() - start) * 1000);
}

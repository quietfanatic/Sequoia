#include "../util/hash.h"

#include "tests.h"

int main (int argc, char** argv) {
    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            run_test(argv[i]);
        }
    }
    else {
        run_all_tests();
    }
    return 0;
}

/* SPDX-License-Identifier: MPL-2.0 */
#include "framework.h"

#include <stdlib.h>

const char *test_vectors_directory(void) {
    const char *directory = getenv("MAELYS_JSON_VECTORS");
    return directory && *directory ? directory : "tests/vectors";
}

static int run_suite(test_suite_t suite, size_t *out_failures) {
    size_t failures = 0u;
    for (size_t i = 0u; i < suite.count; ++i) {
        int failed = suite.cases[i].run();
        printf("%s %s/%s\n", failed ? "FAIL" : "PASS", suite.name,
            suite.cases[i].name);
        failures += failed ? 1u : 0u;
    }
    *out_failures += failures;
    return (int)suite.count;
}

int main(int argc, char **argv) {
    int only_conformance = argc > 1 && strcmp(argv[1], "conformance") == 0;
    size_t failures = 0u;
    int total = 0;
    if (only_conformance) {
        total += run_suite(test_conformance_suite(), &failures);
    } else {
        total += run_suite(test_parser_suite(), &failures);
        total += run_suite(test_reader_suite(), &failures);
        total += run_suite(test_writer_suite(), &failures);
        total += run_suite(test_vectors_suite(), &failures);
        total += run_suite(test_conformance_suite(), &failures);
    }
    printf("%s: %d tests, %zu failures (maelys-json %s)\n",
        failures ? "FAILED" : "PASSED", total, failures, maelys_json_version());
    return failures ? 1 : 0;
}

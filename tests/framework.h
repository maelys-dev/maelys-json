#ifndef MAELYS_JSON_TEST_FRAMEWORK_H
#define MAELYS_JSON_TEST_FRAMEWORK_H

#include "maelys/json.h"

#include <stdio.h>
#include <string.h>

typedef struct test_case {
    const char *name;
    int (*run)(void);
} test_case_t;

typedef struct test_suite {
    const char *name;
    const test_case_t *cases;
    size_t count;
} test_suite_t;

#define TEST_SUITE(identifier, cases) \
    test_suite_t identifier(void) { \
        return (test_suite_t){#identifier, cases, \
            sizeof(cases) / sizeof(cases[0])}; \
    }

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "    %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

#define CHECK_RESULT(expression, expected) do { \
    maelys_json_result_t check_actual_ = (expression); \
    maelys_json_result_t check_expected_ = (expected); \
    if (check_actual_ != check_expected_) { \
        fprintf(stderr, "    %s:%d: %s\n      got \"%s\", expected \"%s\"\n", \
            __FILE__, __LINE__, #expression, \
            maelys_json_result_string(check_actual_), \
            maelys_json_result_string(check_expected_)); \
        return 1; \
    } \
} while (0)

/* Checks bytes, size and the NUL-termination guarantee of a view. */
#define CHECK_VIEW(view, literal) \
    CHECK((view).size == sizeof(literal) - 1u && \
        memcmp((view).data, literal, (view).size) == 0 && \
        (view).data[(view).size] == '\0')

#define CHECK_BYTES(bytes, size, literal) \
    CHECK((size) == sizeof(literal) - 1u && \
        memcmp(bytes, literal, sizeof(literal) - 1u) == 0)

static inline maelys_json_result_t parse_text(
    const char *text, maelys_json_profile_t profile,
    const maelys_json_limits_t *limits, maelys_json_document_t **out_document,
    maelys_json_error_t *out_error) {
    return maelys_json_document_parse(text, strlen(text), profile, limits,
        out_document, out_error);
}

/* Directory holding the canonical vectors (MAELYS_JSON_VECTORS). */
const char *test_vectors_directory(void);

test_suite_t test_parser_suite(void);
test_suite_t test_reader_suite(void);
test_suite_t test_writer_suite(void);
test_suite_t test_vectors_suite(void);
test_suite_t test_conformance_suite(void);

#endif

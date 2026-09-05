/* SPDX-License-Identifier: MPL-2.0 */
/*
 * RFC 8259 conformance. Built-in cases always run. The vendored
 * JSONTestSuite corpus (tests/conformance/JSONTestSuite/test_parsing, or the
 * directory named by MAELYS_JSON_TEST_SUITE) is mandatory: every y_ file must
 * parse, every n_ file must be rejected, and i_ files must not crash. A
 * missing corpus is a failure, never a skip. Documented Maelys deviations
 * from the suite's y_ set are listed in `deviations`.
 */
#define _POSIX_C_SOURCE 200809L

#include "framework.h"

#include <dirent.h>
#include <stdlib.h>

static const char *const accepted[] = {
    "[]", "{}", "\"a\"", "1", "-1", "1.5e10", "[1,2]", "{\"a\":[]}",
    "[\"\\u00e9\"]", "[true,false,null]", "\" \"", "[ 1 ]", "{\"\":0}",
    "[[[[]]]]", "-0", "1E-2", "\"\\ud83d\\ude80\"", "[1,\n2]", "\t[]\r\n",
    "{\"a\":{\"a\":1}}", "[\"\\u0080\"]", "123e65", "[0e+1]", "[\"\\u002c\"]"
};

static const char *const rejected[] = {
    "[1,]", "[,1]", "{\"a\":1,}", "{a:1}", "['a']", "[01]", "[1.]", "[.1]",
    "[+1]", "[1e]", "[-]", "[\"\\x\"]", "[\"\\u12\"]", "[\"a", "[", "]",
    "{\"a\"}", "{\"a\":}", "[1 2]", "nul", "[True]", "[\"\\ud800\"]",
    "[\"tab\there\"]", "[1]x", "[NaN]", "[Infinity]", "[-Infinity]",
    "{\"a\":1,\"a\":2}", "[\"\\u0000\"]", "\xef\xbb\xbf[]", "[0x1]",
    "[\"\\a\"]", "[1,,2]", "{\"a\":1 \"b\":2}", "[\f1]", "\"\xa0\"",
    "[\"\xc3\"]", "{1:2}", "[\"a\"\"b\"]", "{\"a\":1}}", "[1]]", "",
    "/*x*/[]", "[1]//", "{\"a\":\"b\",}", "[\"\\\"]", "\"\\uD800\\uD800\""
};

typedef struct deviation {
    const char *fragment;
    maelys_json_result_t code;
} deviation_t;

/* y_ files JSONTestSuite accepts that Maelys rejects by documented design. */
static const deviation_t deviations[] = {
    {"duplicated_key", MAELYS_JSON_ERR_DUPLICATE_KEY},
    {"null_escape", MAELYS_JSON_ERR_SYNTAX},
    {"escaped_null", MAELYS_JSON_ERR_SYNTAX},
};

static int builtin_cases(void) {
    maelys_json_document_t *document = NULL;
    for (size_t i = 0u; i < sizeof(accepted) / sizeof(accepted[0]); ++i) {
        maelys_json_error_t error;
        maelys_json_result_t result = parse_text(accepted[i],
            MAELYS_JSON_PROFILE_RFC8259, NULL, &document, &error);
        if (result != MAELYS_JSON_OK) {
            fprintf(stderr, "    accepted[%zu] rejected: %s\n", i,
                maelys_json_result_string(result));
            return 1;
        }
        maelys_json_document_release(document);
    }
    for (size_t i = 0u; i < sizeof(rejected) / sizeof(rejected[0]); ++i) {
        document = NULL;
        maelys_json_result_t result = parse_text(rejected[i],
            MAELYS_JSON_PROFILE_RFC8259, NULL, &document, NULL);
        if (result == MAELYS_JSON_OK || document) {
            fprintf(stderr, "    rejected[%zu] accepted\n", i);
            maelys_json_document_release(document);
            return 1;
        }
    }
    return 0;
}

static char *read_file(const char *path, size_t *out_size) {
    FILE *stream = fopen(path, "rb");
    if (!stream) {
        return NULL;
    }
    char *buffer = NULL;
    size_t size = 0u;
    size_t capacity = 0u;
    for (;;) {
        if (size == capacity) {
            capacity = capacity ? capacity * 2u : 4096u;
            char *grown = realloc(buffer, capacity);
            if (!grown) {
                free(buffer);
                fclose(stream);
                return NULL;
            }
            buffer = grown;
        }
        size_t chunk = fread(buffer + size, 1u, capacity - size, stream);
        if (!chunk) {
            break;
        }
        size += chunk;
    }
    fclose(stream);
    *out_size = size;
    return buffer;
}

static maelys_json_result_t expected_deviation(const char *name) {
    for (size_t i = 0u; i < sizeof(deviations) / sizeof(deviations[0]); ++i) {
        if (strstr(name, deviations[i].fragment)) {
            return deviations[i].code;
        }
    }
    return MAELYS_JSON_OK;
}

static int check_suite_file(const char *directory, const char *name,
    const maelys_json_limits_t *limits) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", directory, name);
    size_t size;
    char *input = read_file(path, &size);
    if (!input) {
        fprintf(stderr, "    cannot read %s\n", path);
        return 1;
    }
    maelys_json_document_t *document = NULL;
    maelys_json_result_t result = maelys_json_document_parse(input, size,
        MAELYS_JSON_PROFILE_RFC8259, limits, &document, NULL);
    maelys_json_document_release(document);
    free(input);
    int failed = 0;
    if (name[0] == 'y') {
        maelys_json_result_t deviation = expected_deviation(name);
        failed = deviation ? result != deviation : result != MAELYS_JSON_OK;
    } else if (name[0] == 'n') {
        failed = result == MAELYS_JSON_OK;
    }
    if (failed) {
        fprintf(stderr, "    %s: %s\n", name, maelys_json_result_string(result));
    }
    return failed;
}

#define VENDORED_SUITE "tests/conformance/JSONTestSuite/test_parsing"

static int json_test_suite(void) {
    const char *directory = getenv("MAELYS_JSON_TEST_SUITE");
    if (!directory || !*directory) {
        directory = VENDORED_SUITE;
    }
    DIR *handle = opendir(directory);
    if (!handle) {
        fprintf(stderr, "    cannot open JSONTestSuite corpus at %s\n", directory);
        return 1;
    }
    const maelys_json_limits_t limits = {
        .maximum_bytes = 1u << 20, .maximum_depth = MAELYS_JSON_MAXIMUM_DEPTH,
        .maximum_tokens = 1u << 18
    };
    int failures = 0;
    int files = 0;
    struct dirent *entry;
    while ((entry = readdir(handle)) != NULL) {
        const char *name = entry->d_name;
        size_t length = strlen(name);
        if (length < 6u || strcmp(name + length - 5u, ".json") != 0 ||
            (name[0] != 'y' && name[0] != 'n' && name[0] != 'i')) {
            continue;
        }
        ++files;
        failures += check_suite_file(directory, name, &limits);
    }
    closedir(handle);
    printf("    JSONTestSuite: %d files, %d failures\n", files, failures);
    return files == 0 || failures ? 1 : 0;
}

static const test_case_t cases[] = {
    {"builtin_cases", builtin_cases},
    {"json_test_suite", json_test_suite},
};

TEST_SUITE(test_conformance_suite, cases)

/* SPDX-License-Identifier: MPL-2.0 */
#include "framework.h"

#include <stdlib.h>

static int finish_equals(maelys_json_writer_t *writer, const char *expected) {
    char *output = NULL;
    size_t size = 0u;
    maelys_json_result_t result = maelys_json_writer_finish(writer, &output,
        &size);
    if (result != MAELYS_JSON_OK) {
        fprintf(stderr, "    finish: %s\n", maelys_json_result_string(result));
        return 1;
    }
    int equal = size == strlen(expected) && memcmp(output, expected, size) == 0 &&
        output[size] == '\0';
    if (!equal) {
        fprintf(stderr, "    expected: %s\n    got:      %s\n", expected, output);
    }
    free(output);
    return equal ? 0 : 1;
}

static int canonical_output(void) {
    maelys_json_writer_t *writer = NULL;
    CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, NULL,
        MAELYS_JSON_WRITER_FINAL_NEWLINE, &writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_object_begin(writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_key_cstr(writer, "z"), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_string_cstr(writer, "line\n\"\x01"), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_key(writer, "a", 1u), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_array_begin(writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_u64(writer, 2u), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_i64(writer, -1), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_u64(writer, UINT64_MAX), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_i64(writer, INT64_MIN), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_boolean(writer, 0), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_boolean(writer, 7), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_null(writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_string(writer, "", 0u), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_array_end(writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_object_end(writer), MAELYS_JSON_OK);
    CHECK(maelys_json_writer_status(writer) == MAELYS_JSON_OK);
    CHECK(finish_equals(writer,
        "{\"a\":[2,-1,18446744073709551615,-9223372036854775808,false,true,null,\"\"],"
        "\"z\":\"line\\n\\\"\\u0001\"}\n") == 0);
    CHECK_RESULT(maelys_json_writer_finish(writer, NULL, NULL), MAELYS_JSON_ERR_ARGUMENT);
    char *output;
    size_t size;
    CHECK_RESULT(maelys_json_writer_finish(writer, &output, &size), MAELYS_JSON_ERR_STATE);
    CHECK_RESULT(maelys_json_writer_u64(writer, 1u), MAELYS_JSON_ERR_STATE);
    maelys_json_writer_release(writer);
    return 0;
}

static int key_order(void) {
    maelys_json_writer_t *writer = NULL;
    CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, NULL, 0u,
        &writer), MAELYS_JSON_OK);
    static const char *const keys[] = {
        "b", "\xc3\xa9", "ab", "z", "a", "", "\x01", "A", "aa", "ba", "b0", "~"
    };
    CHECK_RESULT(maelys_json_writer_object_begin(writer), MAELYS_JSON_OK);
    for (size_t i = 0u; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        CHECK_RESULT(maelys_json_writer_key_cstr(writer, keys[i]), MAELYS_JSON_OK);
        CHECK_RESULT(maelys_json_writer_u64(writer, i), MAELYS_JSON_OK);
    }
    CHECK_RESULT(maelys_json_writer_object_end(writer), MAELYS_JSON_OK);
    CHECK(finish_equals(writer,
        "{\"\":5,\"\\u0001\":6,\"A\":7,\"a\":4,\"aa\":8,\"ab\":2,\"b\":0,\"b0\":10,"
        "\"ba\":9,\"z\":3,\"~\":11,\"\xc3\xa9\":1}") == 0);
    maelys_json_writer_release(writer);
    return 0;
}

static int key_order_utf16(void) {
    /* RFC 8785 order: a supplementary character (high surrogate D83D)
     * sorts before U+E000..U+FFFF, unlike UTF-8 byte order. */
    maelys_json_writer_t *writer = NULL;
    CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, NULL, 0u,
        &writer), MAELYS_JSON_OK);
    static const char *const keys[] = {
        "\xef\xbd\xa1",      /* U+FF61 */
        "\xf0\x9f\x98\x80",  /* U+1F600 */
        "\xee\x80\x80",      /* U+E000 */
        "\xed\x9f\xbf",      /* U+D7FF */
        "\xf0\x90\x80\x80",  /* U+10000 */
        "z", "\xc3\xa9",      /* U+00E9 */
        "\xef\xbf\xbd",      /* U+FFFD */
        ("\xf0\x9f\x98\x80" "a"), "a\xf0\x9f\x98\x80", "a"
    };
    CHECK_RESULT(maelys_json_writer_object_begin(writer), MAELYS_JSON_OK);
    for (size_t i = 0u; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        CHECK_RESULT(maelys_json_writer_key_cstr(writer, keys[i]), MAELYS_JSON_OK);
        CHECK_RESULT(maelys_json_writer_u64(writer, i), MAELYS_JSON_OK);
    }
    CHECK_RESULT(maelys_json_writer_object_end(writer), MAELYS_JSON_OK);
    CHECK(finish_equals(writer,
        "{\"a\":10,\"a\xf0\x9f\x98\x80\":9,\"z\":5,\"\xc3\xa9\":6,\"\xed\x9f\xbf\":3,"
        "\"\xf0\x90\x80\x80\":4,\"\xf0\x9f\x98\x80\":1,\"\xf0\x9f\x98\x80" "a\":8,"
        "\"\xee\x80\x80\":2,\"\xef\xbd\xa1\":0,\"\xef\xbf\xbd\":7}") == 0);
    maelys_json_writer_release(writer);
    return 0;
}

static int text_validation(void) {
    maelys_json_writer_t *writer = NULL;
    CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_CONTRACT_ASCII,
        NULL, 0u, &writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_object_begin(writer), MAELYS_JSON_OK);
    static const char *const rejected[] = {"quote\"", "back\\", "\xc3\xa9", "\n", "\x7f"};
    for (size_t i = 0u; i < sizeof(rejected) / sizeof(rejected[0]); ++i) {
        CHECK_RESULT(maelys_json_writer_key_cstr(writer, rejected[i]), MAELYS_JSON_ERR_ARGUMENT);
        CHECK(maelys_json_writer_status(writer) == MAELYS_JSON_OK);
    }
    CHECK_RESULT(maelys_json_writer_key_cstr(writer, "ok"), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_string_cstr(writer, "\xc3\xa9"), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_writer_string_cstr(writer, "a/b ~"), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_object_end(writer), MAELYS_JSON_OK);
    CHECK(finish_equals(writer, "{\"ok\":\"a/b ~\"}") == 0);
    maelys_json_writer_release(writer);

    CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, NULL, 0u,
        &writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_string(writer, "a\0b", 3u), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_writer_string(writer, "\xc0\x80", 2u), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_writer_string(writer, NULL, 1u), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_writer_string_cstr(writer, NULL), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_writer_key_cstr(writer, NULL), MAELYS_JSON_ERR_ARGUMENT);
    CHECK(maelys_json_writer_status(writer) == MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_string(writer, NULL, 0u), MAELYS_JSON_OK);
    CHECK(finish_equals(writer, "\"\"") == 0);
    maelys_json_writer_release(writer);
    return 0;
}

static int limits(void) {
    maelys_json_limits_t limits = {.maximum_bytes = 3u};
    maelys_json_writer_t *writer = NULL;
    CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, &limits,
        MAELYS_JSON_WRITER_FINAL_NEWLINE, &writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_array_begin(writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_u64(writer, 1u), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_array_end(writer), MAELYS_JSON_OK);
    char *output = (char *)1;
    size_t size = 9u;
    CHECK_RESULT(maelys_json_writer_finish(writer, &output, &size), MAELYS_JSON_ERR_LIMIT);
    CHECK(output == (char *)1 && size == 9u);
    CHECK(maelys_json_writer_status(writer) == MAELYS_JSON_ERR_LIMIT);
    maelys_json_writer_release(writer);
    limits.maximum_bytes = 4u;
    CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, &limits,
        MAELYS_JSON_WRITER_FINAL_NEWLINE, &writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_array_begin(writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_u64(writer, 1u), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_array_end(writer), MAELYS_JSON_OK);
    CHECK(finish_equals(writer, "[1]\n") == 0);
    maelys_json_writer_release(writer);

    limits.maximum_bytes = 4u;
    CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, &limits,
        0u, &writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_string(writer, "12345", 5u), MAELYS_JSON_ERR_LIMIT);
    CHECK_RESULT(maelys_json_writer_string(writer, "1", 1u), MAELYS_JSON_ERR_STATE);
    maelys_json_writer_release(writer);

    limits = (maelys_json_limits_t){.maximum_depth = 2u};
    CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, &limits,
        0u, &writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_array_begin(writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_array_begin(writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_array_begin(writer), MAELYS_JSON_ERR_LIMIT);
    maelys_json_writer_release(writer);

    limits = (maelys_json_limits_t){.maximum_tokens = 3u};
    CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, &limits,
        0u, &writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_array_begin(writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_u64(writer, 1u), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_u64(writer, 2u), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_u64(writer, 3u), MAELYS_JSON_ERR_LIMIT);
    maelys_json_writer_release(writer);

    limits = (maelys_json_limits_t){.maximum_depth = MAELYS_JSON_MAXIMUM_DEPTH + 1u};
    CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, &limits,
        0u, &writer), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, NULL,
        1u << 7, &writer), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_writer_create((maelys_json_profile_t)3, NULL, 0u,
        &writer), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, NULL, 0u,
        NULL), MAELYS_JSON_ERR_ARGUMENT);
    return 0;
}

static int duplicate_keys(void) {
    maelys_json_writer_t *writer = NULL;
    CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, NULL, 0u,
        &writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_array_begin(writer), MAELYS_JSON_OK);
    for (int i = 0; i < 2; ++i) {
        CHECK_RESULT(maelys_json_writer_object_begin(writer), MAELYS_JSON_OK);
        CHECK_RESULT(maelys_json_writer_key_cstr(writer, "same"), MAELYS_JSON_OK);
        CHECK_RESULT(maelys_json_writer_object_begin(writer), MAELYS_JSON_OK);
        CHECK_RESULT(maelys_json_writer_key_cstr(writer, "same"), MAELYS_JSON_OK);
        CHECK_RESULT(maelys_json_writer_u64(writer, (uint64_t)i), MAELYS_JSON_OK);
        CHECK_RESULT(maelys_json_writer_object_end(writer), MAELYS_JSON_OK);
        CHECK_RESULT(maelys_json_writer_object_end(writer), MAELYS_JSON_OK);
    }
    CHECK_RESULT(maelys_json_writer_array_end(writer), MAELYS_JSON_OK);
    CHECK(finish_equals(writer, "[{\"same\":{\"same\":0}},{\"same\":{\"same\":1}}]") == 0);
    maelys_json_writer_release(writer);

    CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, NULL, 0u,
        &writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_object_begin(writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_key_cstr(writer, "b"), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_u64(writer, 1u), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_key_cstr(writer, "b"), MAELYS_JSON_ERR_DUPLICATE_KEY);
    CHECK(maelys_json_writer_status(writer) == MAELYS_JSON_ERR_DUPLICATE_KEY);
    CHECK_RESULT(maelys_json_writer_key_cstr(writer, "c"), MAELYS_JSON_ERR_STATE);
    CHECK_RESULT(maelys_json_writer_object_end(writer), MAELYS_JSON_ERR_STATE);
    char *output;
    size_t size;
    CHECK_RESULT(maelys_json_writer_finish(writer, &output, &size), MAELYS_JSON_ERR_STATE);
    CHECK(maelys_json_writer_status(writer) == MAELYS_JSON_ERR_DUPLICATE_KEY);
    maelys_json_writer_release(writer);
    return 0;
}

typedef maelys_json_result_t (*writer_step_fn)(maelys_json_writer_t *writer);

#define STEP(call) do { \
    maelys_json_result_t step_result_ = (call); \
    if (step_result_ != MAELYS_JSON_OK) { \
        return step_result_; \
    } \
} while (0)

static maelys_json_result_t step_value_without_key(maelys_json_writer_t *w) {
    STEP(maelys_json_writer_object_begin(w));
    return maelys_json_writer_u64(w, 1u);
}
static maelys_json_result_t step_key_in_array(maelys_json_writer_t *w) {
    STEP(maelys_json_writer_array_begin(w));
    return maelys_json_writer_key_cstr(w, "k");
}
static maelys_json_result_t step_key_at_root(maelys_json_writer_t *w) {
    return maelys_json_writer_key_cstr(w, "k");
}
static maelys_json_result_t step_end_mismatch(maelys_json_writer_t *w) {
    STEP(maelys_json_writer_object_begin(w));
    return maelys_json_writer_array_end(w);
}
static maelys_json_result_t step_end_at_root(maelys_json_writer_t *w) {
    return maelys_json_writer_array_end(w);
}
static maelys_json_result_t step_second_root(maelys_json_writer_t *w) {
    STEP(maelys_json_writer_null(w));
    return maelys_json_writer_null(w);
}
static maelys_json_result_t step_double_key(maelys_json_writer_t *w) {
    STEP(maelys_json_writer_object_begin(w));
    STEP(maelys_json_writer_key_cstr(w, "a"));
    return maelys_json_writer_key_cstr(w, "b");
}
static maelys_json_result_t step_end_with_pending_key(maelys_json_writer_t *w) {
    STEP(maelys_json_writer_object_begin(w));
    STEP(maelys_json_writer_key_cstr(w, "a"));
    return maelys_json_writer_object_end(w);
}
static maelys_json_result_t step_finish_open(maelys_json_writer_t *w) {
    char *o;
    size_t n;
    STEP(maelys_json_writer_array_begin(w));
    return maelys_json_writer_finish(w, &o, &n);
}
static maelys_json_result_t step_finish_empty(maelys_json_writer_t *w) {
    char *o;
    size_t n;
    return maelys_json_writer_finish(w, &o, &n);
}

static int state_errors(void) {
    static const writer_step_fn steps[] = {
        step_value_without_key, step_key_in_array, step_key_at_root,
        step_end_mismatch, step_end_at_root, step_second_root, step_double_key,
        step_end_with_pending_key, step_finish_open, step_finish_empty
    };
    for (size_t i = 0u; i < sizeof(steps) / sizeof(steps[0]); ++i) {
        maelys_json_writer_t *writer = NULL;
        CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, NULL,
            0u, &writer), MAELYS_JSON_OK);
        maelys_json_result_t failed = steps[i](writer);
        if (failed != MAELYS_JSON_ERR_STATE ||
            maelys_json_writer_status(writer) != MAELYS_JSON_ERR_STATE) {
            fprintf(stderr, "    step %zu: %s, status %s\n", i,
                maelys_json_result_string(failed),
                maelys_json_result_string(maelys_json_writer_status(writer)));
            return 1;
        }
        /* Sticky: everything but release and status now reports STATE. */
        CHECK_RESULT(maelys_json_writer_object_begin(writer), MAELYS_JSON_ERR_STATE);
        CHECK_RESULT(maelys_json_writer_array_begin(writer), MAELYS_JSON_ERR_STATE);
        CHECK_RESULT(maelys_json_writer_key_cstr(writer, "k"), MAELYS_JSON_ERR_STATE);
        CHECK_RESULT(maelys_json_writer_string_cstr(writer, "s"), MAELYS_JSON_ERR_STATE);
        CHECK_RESULT(maelys_json_writer_u64(writer, 0u), MAELYS_JSON_ERR_STATE);
        CHECK_RESULT(maelys_json_writer_i64(writer, 0), MAELYS_JSON_ERR_STATE);
        CHECK_RESULT(maelys_json_writer_boolean(writer, 1), MAELYS_JSON_ERR_STATE);
        CHECK_RESULT(maelys_json_writer_null(writer), MAELYS_JSON_ERR_STATE);
        CHECK_RESULT(maelys_json_writer_object_end(writer), MAELYS_JSON_ERR_STATE);
        CHECK_RESULT(maelys_json_writer_array_end(writer), MAELYS_JSON_ERR_STATE);
        maelys_json_writer_release(writer);
    }
    return 0;
}

static int null_writer(void) {
    char *output;
    size_t size;
    CHECK_RESULT(maelys_json_writer_object_begin(NULL), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_writer_object_end(NULL), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_writer_array_begin(NULL), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_writer_array_end(NULL), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_writer_key(NULL, "k", 1u), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_writer_string(NULL, "s", 1u), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_writer_u64(NULL, 1u), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_writer_i64(NULL, 1), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_writer_boolean(NULL, 1), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_writer_null(NULL), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_writer_finish(NULL, &output, &size), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_writer_finish_file(NULL, stdout), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_writer_value(NULL, NULL, 0u), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_writer_status(NULL), MAELYS_JSON_ERR_ARGUMENT);
    maelys_json_writer_release(NULL);
    return 0;
}

static int build_sample(maelys_json_writer_t *writer) {
    return maelys_json_writer_object_begin(writer) ||
        maelys_json_writer_key_cstr(writer, "z") ||
        maelys_json_writer_object_begin(writer) ||
        maelys_json_writer_object_end(writer) ||
        maelys_json_writer_key_cstr(writer, "a") ||
        maelys_json_writer_array_begin(writer) ||
        maelys_json_writer_u64(writer, 1u) ||
        maelys_json_writer_string_cstr(writer, "\xc3\xa9\xf0\x9f\x9a\x80" "e") ||
        maelys_json_writer_array_begin(writer) ||
        maelys_json_writer_array_end(writer) ||
        maelys_json_writer_array_end(writer) ||
        maelys_json_writer_object_end(writer);
}

static int indent_and_ascii(void) {
    maelys_json_writer_t *writer = NULL;
    CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, NULL,
        MAELYS_JSON_WRITER_INDENT | MAELYS_JSON_WRITER_FINAL_NEWLINE, &writer),
        MAELYS_JSON_OK);
    CHECK(build_sample(writer) == 0);
    CHECK(finish_equals(writer,
        "{\n"
        "  \"a\": [\n"
        "    1,\n"
        "    \"\xc3\xa9\xf0\x9f\x9a\x80" "e\",\n"
        "    []\n"
        "  ],\n"
        "  \"z\": {}\n"
        "}\n") == 0);
    maelys_json_writer_release(writer);
    CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, NULL,
        MAELYS_JSON_WRITER_ASCII, &writer), MAELYS_JSON_OK);
    CHECK(build_sample(writer) == 0);
    CHECK(finish_equals(writer,
        "{\"a\":[1,\"\\u00e9\\ud83d\\ude80" "e\",[]],\"z\":{}}") == 0);
    maelys_json_writer_release(writer);
    CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, NULL, 0u,
        &writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_object_begin(writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_key_cstr(writer, "\xc3\xa9"), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_string_cstr(writer, "\x7f\xe2\x82\xac"), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_object_end(writer), MAELYS_JSON_OK);
    CHECK(finish_equals(writer, "{\"\xc3\xa9\":\"\x7f\xe2\x82\xac\"}") == 0);
    maelys_json_writer_release(writer);
    return 0;
}

static int bridge(void) {
    maelys_json_document_t *document = NULL;
    CHECK_RESULT(parse_text(" {\"b\" : [ 1 , -2 , \"x\\u00e9\" , true , null , {} ] , \"a\" : -0 } ",
        MAELYS_JSON_PROFILE_RFC8259, NULL, &document, NULL), MAELYS_JSON_OK);
    maelys_json_writer_t *writer = NULL;
    CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, NULL, 0u,
        &writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_array_begin(writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_value(writer, document,
        maelys_json_document_root(document)), MAELYS_JSON_OK);
    maelys_json_value_t b;
    CHECK_RESULT(maelys_json_object_get(document, 0u, "b", &b), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_value(writer, document, b), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_value(writer, document, 999u), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_writer_value(writer, NULL, 0u), MAELYS_JSON_ERR_ARGUMENT);
    CHECK(maelys_json_writer_status(writer) == MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_array_end(writer), MAELYS_JSON_OK);
    CHECK(finish_equals(writer,
        "[{\"a\":0,\"b\":[1,-2,\"x\xc3\xa9\",true,null,{}]},[1,-2,\"x\xc3\xa9\",true,null,{}]]") == 0);
    maelys_json_writer_release(writer);

    CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_CONTRACT_ASCII,
        NULL, 0u, &writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_value(writer, document, 0u), MAELYS_JSON_ERR_ARGUMENT);
    CHECK(maelys_json_writer_status(writer) == MAELYS_JSON_ERR_STATE);
    maelys_json_writer_release(writer);
    maelys_json_document_release(document);

    CHECK_RESULT(parse_text("[1.5]", MAELYS_JSON_PROFILE_RFC8259, NULL,
        &document, NULL), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, NULL, 0u,
        &writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_value(writer, document, 0u), MAELYS_JSON_ERR_NOT_INTEGER);
    CHECK(maelys_json_writer_status(writer) == MAELYS_JSON_ERR_NOT_INTEGER);
    maelys_json_writer_release(writer);
    maelys_json_document_release(document);

    maelys_json_limits_t limits = {.maximum_depth = 2u};
    CHECK_RESULT(parse_text("[[[]]]", MAELYS_JSON_PROFILE_RFC8259, NULL,
        &document, NULL), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, &limits,
        0u, &writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_value(writer, document, 0u), MAELYS_JSON_ERR_LIMIT);
    maelys_json_writer_release(writer);
    maelys_json_document_release(document);
    return 0;
}

static int canonical_check(const char *text, maelys_json_profile_t profile,
    unsigned int flags, int expected) {
    maelys_json_document_t *document = NULL;
    if (parse_text(text, profile, NULL, &document, NULL) != MAELYS_JSON_OK) {
        fprintf(stderr, "    parse failed: %s\n", text);
        return 1;
    }
    int canonical = -1;
    maelys_json_result_t result = maelys_json_document_is_canonical(document,
        flags, &canonical);
    maelys_json_document_release(document);
    if (result != MAELYS_JSON_OK || canonical != expected) {
        fprintf(stderr, "    %s: %s, canonical=%d, expected %d\n", text,
            maelys_json_result_string(result), canonical, expected);
        return 1;
    }
    return 0;
}

static int is_canonical(void) {
    const maelys_json_profile_t rfc = MAELYS_JSON_PROFILE_RFC8259;
    const maelys_json_profile_t ascii = MAELYS_JSON_PROFILE_CONTRACT_ASCII;
    CHECK(canonical_check("{\"a\":[1,-2,true,null,\"x\"],\"b\":{}}", rfc, 0u, 1) == 0);
    CHECK(canonical_check("{\"a\":1}", ascii, 0u, 1) == 0);
    CHECK(canonical_check("{ \"a\" : 1 }", ascii, 0u, 0) == 0);
    CHECK(canonical_check("{\"a\":-0}", ascii, 0u, 0) == 0);
    CHECK(canonical_check("{\"a\":0.0}", ascii, 0u, 0) == 0);
    CHECK(canonical_check("{\"a\":1e2}", rfc, 0u, 0) == 0);
    CHECK(canonical_check("{\"b\":1,\"a\":2}", rfc, 0u, 0) == 0);
    CHECK(canonical_check("[\"\\u0041\"]", rfc, 0u, 0) == 0);
    CHECK(canonical_check("[\"\\/\"]", rfc, 0u, 0) == 0);
    CHECK(canonical_check("[18446744073709551616]", rfc, 0u, 0) == 0);
    CHECK(canonical_check("[1]\n", rfc, MAELYS_JSON_WRITER_FINAL_NEWLINE, 1) == 0);
    CHECK(canonical_check("[1]\n", rfc, 0u, 0) == 0);
    CHECK(canonical_check("[1]", rfc, MAELYS_JSON_WRITER_FINAL_NEWLINE, 0) == 0);
    CHECK(canonical_check("1", rfc, 0u, 1) == 0);
    CHECK(canonical_check("\"\xc3\xa9\"", rfc, 0u, 1) == 0);
    maelys_json_document_t *document = NULL;
    CHECK_RESULT(parse_text("[1]", rfc, NULL, &document, NULL), MAELYS_JSON_OK);
    int canonical;
    CHECK_RESULT(maelys_json_document_is_canonical(document,
        MAELYS_JSON_WRITER_INDENT, &canonical), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_document_is_canonical(document, 0u, NULL), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_document_is_canonical(NULL, 0u, &canonical), MAELYS_JSON_ERR_ARGUMENT);
    maelys_json_document_release(document);
    return 0;
}

static int object_begin_except(void) {
    maelys_json_document_t *document = NULL;
    CHECK_RESULT(parse_text("{\"retries\":1,\"name\":\"x\",\"nested\":{\"k\":[1]},\"drop\":true}",
        MAELYS_JSON_PROFILE_RFC8259, NULL, &document, NULL), MAELYS_JSON_OK);
    maelys_json_writer_t *writer = NULL;
    CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, NULL, 0u,
        &writer), MAELYS_JSON_OK);
    static const char *const excluded[] = {"retries", "drop", "absent"};
    CHECK_RESULT(maelys_json_writer_object_begin_except(writer, document, 0u,
        excluded, 3u), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_key_cstr(writer, "retries"), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_u64(writer, 2u), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_key_cstr(writer, "added"), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_null(writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_object_end(writer), MAELYS_JSON_OK);
    CHECK(finish_equals(writer,
        "{\"added\":null,\"name\":\"x\",\"nested\":{\"k\":[1]},\"retries\":2}") == 0);
    maelys_json_writer_release(writer);

    /* No exclusion copies everything; the object stays open. */
    CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, NULL, 0u,
        &writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_object_begin_except(writer, document, 0u,
        NULL, 0u), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_key_cstr(writer, "name"), MAELYS_JSON_ERR_DUPLICATE_KEY);
    maelys_json_writer_release(writer);

    /* Argument errors leave the writer untouched. */
    CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, NULL, 0u,
        &writer), MAELYS_JSON_OK);
    static const char *const with_null[] = {"a", NULL};
    CHECK_RESULT(maelys_json_writer_object_begin_except(writer, document, 0u,
        with_null, 2u), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_writer_object_begin_except(writer, document, 0u,
        NULL, 1u), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_writer_object_begin_except(NULL, document, 0u,
        NULL, 0u), MAELYS_JSON_ERR_ARGUMENT);
    maelys_json_value_t nested;
    CHECK_RESULT(maelys_json_object_get(document, 0u, "nested", &nested), MAELYS_JSON_OK);
    maelys_json_value_t array;
    CHECK_RESULT(maelys_json_object_get(document, nested, "k", &array), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_object_begin_except(writer, document, array,
        NULL, 0u), MAELYS_JSON_ERR_ARGUMENT);
    CHECK(maelys_json_writer_status(writer) == MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_object_begin_except(writer, document, nested,
        NULL, 0u), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_object_end(writer), MAELYS_JSON_OK);
    CHECK(finish_equals(writer, "{\"k\":[1]}") == 0);
    maelys_json_writer_release(writer);
    maelys_json_document_release(document);

    /* A profile mismatch mid-copy is sticky, like writer_value. */
    CHECK_RESULT(parse_text("{\"a\":\"\xc3\xa9\"}", MAELYS_JSON_PROFILE_RFC8259,
        NULL, &document, NULL), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_CONTRACT_ASCII,
        NULL, 0u, &writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_object_begin_except(writer, document, 0u,
        NULL, 0u), MAELYS_JSON_ERR_ARGUMENT);
    CHECK(maelys_json_writer_status(writer) == MAELYS_JSON_ERR_STATE);
    maelys_json_writer_release(writer);
    maelys_json_document_release(document);
    return 0;
}

static int finish_file(void) {
    maelys_json_writer_t *writer = NULL;
    CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, NULL,
        MAELYS_JSON_WRITER_FINAL_NEWLINE, &writer), MAELYS_JSON_OK);
    CHECK(build_sample(writer) == 0);
    FILE *stream = tmpfile();
    CHECK(stream != NULL);
    CHECK_RESULT(maelys_json_writer_finish_file(writer, NULL), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_writer_finish_file(writer, stream), MAELYS_JSON_OK);
    rewind(stream);
    char buffer[128];
    size_t size = fread(buffer, 1u, sizeof(buffer), stream);
    fclose(stream);
    CHECK_BYTES(buffer, size, "{\"a\":[1,\"\xc3\xa9\xf0\x9f\x9a\x80" "e\",[]],\"z\":{}}\n");
    CHECK_RESULT(maelys_json_writer_finish_file(writer, stdout), MAELYS_JSON_ERR_STATE);
    maelys_json_writer_release(writer);
    return 0;
}

static int growth(void) {
    /* Output well past the initial 128-byte buffer, and node growth. */
    maelys_json_writer_t *writer = NULL;
    CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, NULL, 0u,
        &writer), MAELYS_JSON_OK);
    char text[200];
    memset(text, 'x', sizeof(text));
    CHECK_RESULT(maelys_json_writer_array_begin(writer), MAELYS_JSON_OK);
    for (int i = 0; i < 200; ++i) {
        CHECK_RESULT(maelys_json_writer_string(writer, text, sizeof(text)), MAELYS_JSON_OK);
    }
    CHECK_RESULT(maelys_json_writer_array_end(writer), MAELYS_JSON_OK);
    char *output;
    size_t size;
    CHECK_RESULT(maelys_json_writer_finish(writer, &output, &size), MAELYS_JSON_OK);
    CHECK(size == 2u + 200u * 203u - 1u);
    maelys_json_document_t *document;
    CHECK_RESULT(maelys_json_document_parse(output, size,
        MAELYS_JSON_PROFILE_RFC8259, NULL, &document, NULL), MAELYS_JSON_OK);
    maelys_json_document_release(document);
    free(output);
    maelys_json_writer_release(writer);
    return 0;
}

static int many_keys(void) {
    /* Grows the writer key set past its initial capacity, then checks the
     * sort and the duplicate detection still hold. */
    maelys_json_writer_t *writer = NULL;
    CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, NULL, 0u,
        &writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_object_begin(writer), MAELYS_JSON_OK);
    for (int i = 199; i >= 0; --i) {
        char key[16];
        snprintf(key, sizeof(key), "k%03d", i);
        CHECK_RESULT(maelys_json_writer_key_cstr(writer, key), MAELYS_JSON_OK);
        CHECK_RESULT(maelys_json_writer_u64(writer, (uint64_t)i), MAELYS_JSON_OK);
    }
    CHECK_RESULT(maelys_json_writer_key_cstr(writer, "k123"), MAELYS_JSON_ERR_DUPLICATE_KEY);
    maelys_json_writer_release(writer);

    CHECK_RESULT(maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, NULL, 0u,
        &writer), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_writer_object_begin(writer), MAELYS_JSON_OK);
    for (int i = 199; i >= 0; --i) {
        char key[16];
        snprintf(key, sizeof(key), "k%03d", i);
        CHECK_RESULT(maelys_json_writer_key_cstr(writer, key), MAELYS_JSON_OK);
        CHECK_RESULT(maelys_json_writer_u64(writer, (uint64_t)i), MAELYS_JSON_OK);
    }
    CHECK_RESULT(maelys_json_writer_object_end(writer), MAELYS_JSON_OK);
    char *output;
    size_t size;
    CHECK_RESULT(maelys_json_writer_finish(writer, &output, &size), MAELYS_JSON_OK);
    CHECK(strncmp(output, "{\"k000\":0,\"k001\":1,\"k002\":2,", 28) == 0);
    maelys_json_document_t *document;
    CHECK_RESULT(maelys_json_document_parse(output, size,
        MAELYS_JSON_PROFILE_RFC8259, NULL, &document, NULL), MAELYS_JSON_OK);
    uint64_t number;
    CHECK_RESULT(maelys_json_object_get_u64(document, 0u, "k150", &number), MAELYS_JSON_OK);
    CHECK(number == 150u);
    maelys_json_document_release(document);
    free(output);
    maelys_json_writer_release(writer);
    return 0;
}

static const test_case_t cases[] = {
    {"many_keys", many_keys},
    {"canonical_output", canonical_output},
    {"key_order", key_order},
    {"key_order_utf16", key_order_utf16},
    {"text_validation", text_validation},
    {"limits", limits},
    {"duplicate_keys", duplicate_keys},
    {"state_errors", state_errors},
    {"null_writer", null_writer},
    {"indent_and_ascii", indent_and_ascii},
    {"bridge", bridge},
    {"is_canonical", is_canonical},
    {"object_begin_except", object_begin_except},
    {"finish_file", finish_file},
    {"growth", growth},
};

TEST_SUITE(test_writer_suite, cases)

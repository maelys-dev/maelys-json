/* SPDX-License-Identifier: MPL-2.0 */
#include "framework.h"

#include <stdlib.h>

static int rfc_document(void) {
    static const char input[] =
        " {\"array\" : [true, null, 18446744073709551615],\n"
        "\"escaped\":\"A\\u00e9\\ud83d\\ude80\", \"signed\":-42} ";
    maelys_json_document_t *document = NULL;
    maelys_json_error_t error;
    CHECK_RESULT(parse_text(input, MAELYS_JSON_PROFILE_RFC8259, NULL,
        &document, &error), MAELYS_JSON_OK);
    CHECK(error.code == MAELYS_JSON_OK);
    maelys_json_value_t root = maelys_json_document_root(document);
    CHECK(root == 0u);
    CHECK(maelys_json_value_type(document, root) == MAELYS_JSON_TYPE_OBJECT);
    maelys_json_view_t view;
    CHECK_RESULT(maelys_json_object_get_string(document, root, "escaped",
        &view), MAELYS_JSON_OK);
    CHECK_VIEW(view, "A\xc3\xa9\xf0\x9f\x9a\x80");
    maelys_json_document_release(document);
    return 0;
}

static int contract_profile(void) {
    maelys_json_document_t *document = NULL;
    CHECK_RESULT(parse_text("{\"schema\":\"driver/v2\",\"n\":2}",
        MAELYS_JSON_PROFILE_CONTRACT_ASCII, NULL, &document, NULL),
        MAELYS_JSON_OK);
    maelys_json_document_release(document);
    static const char *const rejected[] = {
        "{\"a\":\"\\u0061\"}", "{\"a\":\"\\\"\"}", "{\"a\":\"\xc3\xa9\"}",
        "{\"a\":\"\x7f\"}", "{\"a\":\"\\n\"}", "\"\\/\""
    };
    for (size_t i = 0u; i < sizeof(rejected) / sizeof(rejected[0]); ++i) {
        document = (maelys_json_document_t *)1;
        CHECK_RESULT(parse_text(rejected[i],
            MAELYS_JSON_PROFILE_CONTRACT_ASCII, NULL, &document, NULL),
            MAELYS_JSON_ERR_SYNTAX);
        CHECK(document == NULL);
    }
    return 0;
}

static int empty_and_whitespace(void) {
    maelys_json_document_t *document = NULL;
    maelys_json_error_t error;
    CHECK_RESULT(maelys_json_document_parse(NULL, 0u,
        MAELYS_JSON_PROFILE_RFC8259, NULL, &document, &error),
        MAELYS_JSON_ERR_SYNTAX);
    CHECK(error.offset == 0u && error.line == 1u && error.column == 1u);
    CHECK_RESULT(parse_text("", MAELYS_JSON_PROFILE_RFC8259, NULL, &document,
        &error), MAELYS_JSON_ERR_SYNTAX);
    CHECK_RESULT(parse_text(" \t\r\n", MAELYS_JSON_PROFILE_RFC8259, NULL,
        &document, &error), MAELYS_JSON_ERR_SYNTAX);
    CHECK(error.offset == 4u && error.line == 2u && error.column == 1u);
    CHECK_RESULT(parse_text("[\f1]", MAELYS_JSON_PROFILE_RFC8259, NULL,
        &document, &error), MAELYS_JSON_ERR_SYNTAX);
    CHECK_RESULT(parse_text("[\xc2\xa0" "1]", MAELYS_JSON_PROFILE_RFC8259,
        NULL, &document, &error), MAELYS_JSON_ERR_SYNTAX);
    CHECK_RESULT(parse_text("\xef\xbb\xbf{}", MAELYS_JSON_PROFILE_RFC8259,
        NULL, &document, &error), MAELYS_JSON_ERR_SYNTAX);
    CHECK(error.offset == 0u);
    return 0;
}

static int trailing_data(void) {
    maelys_json_document_t *document = NULL;
    maelys_json_error_t error;
    CHECK_RESULT(parse_text("{\"a\":1} x", MAELYS_JSON_PROFILE_RFC8259, NULL,
        &document, &error), MAELYS_JSON_ERR_SYNTAX);
    CHECK(error.offset == 8u && error.column == 9u);
    CHECK_RESULT(parse_text("1 2", MAELYS_JSON_PROFILE_RFC8259, NULL,
        &document, &error), MAELYS_JSON_ERR_SYNTAX);
    CHECK_RESULT(parse_text("[1]]", MAELYS_JSON_PROFILE_RFC8259, NULL,
        &document, &error), MAELYS_JSON_ERR_SYNTAX);
    CHECK(document == NULL);
    return 0;
}

typedef struct position_case {
    const char *input;
    maelys_json_result_t code;
    size_t offset;
    size_t line;
    size_t column;
} position_case_t;

static int error_positions(void) {
    static const position_case_t cases[] = {
        {"{\"a\" 1}", MAELYS_JSON_ERR_SYNTAX, 5u, 1u, 6u},
        {"{\n  \"a\":1,\n  \"\\u0061\":2}", MAELYS_JSON_ERR_DUPLICATE_KEY, 13u, 3u, 3u},
        {"{\r\n\"a\":1,\r\n\"a\":2}", MAELYS_JSON_ERR_DUPLICATE_KEY, 11u, 3u, 1u},
        {"[\"abc", MAELYS_JSON_ERR_SYNTAX, 1u, 1u, 2u},
        {"[\"a\\qb\"]", MAELYS_JSON_ERR_SYNTAX, 3u, 1u, 4u},
        {"[\"ab\xc0\x80\"]", MAELYS_JSON_ERR_UTF8, 4u, 1u, 5u},
        {"[\"a\tb\"]", MAELYS_JSON_ERR_SYNTAX, 3u, 1u, 4u},
        {"[1,]", MAELYS_JSON_ERR_SYNTAX, 3u, 1u, 4u},
        {"[1 2]", MAELYS_JSON_ERR_SYNTAX, 3u, 1u, 4u},
        {"{\"a\":1 \"b\":2}", MAELYS_JSON_ERR_SYNTAX, 7u, 1u, 8u},
        {"[tru]", MAELYS_JSON_ERR_SYNTAX, 1u, 1u, 2u},
        {"[\"\xc3\xa9\", x]", MAELYS_JSON_ERR_SYNTAX, 7u, 1u, 8u},
        {"{\"a\":[1,\n2", MAELYS_JSON_ERR_SYNTAX, 10u, 2u, 2u},
        {"[\"\\ud800x\"]", MAELYS_JSON_ERR_SYNTAX, 2u, 1u, 3u},
    };
    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        maelys_json_document_t *document = NULL;
        maelys_json_error_t error;
        maelys_json_result_t result = parse_text(cases[i].input,
            MAELYS_JSON_PROFILE_RFC8259, NULL, &document, &error);
        if (result != cases[i].code || error.code != cases[i].code ||
            error.offset != cases[i].offset || error.line != cases[i].line ||
            error.column != cases[i].column) {
            fprintf(stderr, "    case %zu: %s offset=%zu line=%zu column=%zu\n",
                i, maelys_json_result_string(result), error.offset,
                error.line, error.column);
            return 1;
        }
    }
    return 0;
}

static int nested_text(char *buffer, size_t depth, const char *leaf) {
    size_t leaf_size = strlen(leaf);
    memset(buffer, '[', depth);
    memcpy(buffer + depth, leaf, leaf_size);
    memset(buffer + depth + leaf_size, ']', depth);
    buffer[2u * depth + leaf_size] = '\0';
    return 0;
}

static int depth_semantics(void) {
    maelys_json_limits_t limits = {.maximum_depth = 2u};
    static const char *const accepted[] = {"[[]]", "[[0]]", "{\"a\":{\"b\":1}}", "1"};
    static const struct { const char *input; size_t offset; } rejected[] = {
        {"[[[]]]", 2u}, {"[[[0]]]", 2u}, {"{\"a\":{\"b\":{}}}", 10u}
    };
    maelys_json_document_t *document = NULL;
    maelys_json_error_t error;
    for (size_t i = 0u; i < sizeof(accepted) / sizeof(accepted[0]); ++i) {
        CHECK_RESULT(parse_text(accepted[i], MAELYS_JSON_PROFILE_RFC8259,
            &limits, &document, &error), MAELYS_JSON_OK);
        maelys_json_document_release(document);
    }
    for (size_t i = 0u; i < sizeof(rejected) / sizeof(rejected[0]); ++i) {
        CHECK_RESULT(parse_text(rejected[i].input, MAELYS_JSON_PROFILE_RFC8259,
            &limits, &document, &error), MAELYS_JSON_ERR_LIMIT);
        CHECK(error.code == MAELYS_JSON_ERR_LIMIT &&
            error.offset == rejected[i].offset);
    }
    char buffer[1024];
    nested_text(buffer, MAELYS_JSON_DEFAULT_MAXIMUM_DEPTH, "");
    CHECK_RESULT(parse_text(buffer, MAELYS_JSON_PROFILE_RFC8259, NULL,
        &document, &error), MAELYS_JSON_OK);
    maelys_json_document_release(document);
    nested_text(buffer, MAELYS_JSON_DEFAULT_MAXIMUM_DEPTH + 1u, "");
    CHECK_RESULT(parse_text(buffer, MAELYS_JSON_PROFILE_RFC8259, NULL,
        &document, &error), MAELYS_JSON_ERR_LIMIT);
    limits.maximum_depth = MAELYS_JSON_MAXIMUM_DEPTH;
    nested_text(buffer, MAELYS_JSON_MAXIMUM_DEPTH, "0");
    CHECK_RESULT(parse_text(buffer, MAELYS_JSON_PROFILE_RFC8259, &limits,
        &document, &error), MAELYS_JSON_OK);
    maelys_json_document_release(document);
    nested_text(buffer, MAELYS_JSON_MAXIMUM_DEPTH + 1u, "0");
    CHECK_RESULT(parse_text(buffer, MAELYS_JSON_PROFILE_RFC8259, &limits,
        &document, &error), MAELYS_JSON_ERR_LIMIT);
    limits.maximum_depth = MAELYS_JSON_MAXIMUM_DEPTH + 1u;
    CHECK_RESULT(parse_text("[]", MAELYS_JSON_PROFILE_RFC8259, &limits,
        &document, &error), MAELYS_JSON_ERR_ARGUMENT);
    return 0;
}

static int token_and_byte_limits(void) {
    maelys_json_limits_t limits = {.maximum_bytes = 8u, .maximum_tokens = 4u};
    maelys_json_document_t *document = NULL;
    maelys_json_error_t error;
    CHECK_RESULT(parse_text("[1,2,3]", MAELYS_JSON_PROFILE_RFC8259, &limits,
        &document, &error), MAELYS_JSON_OK);
    maelys_json_document_release(document);
    CHECK_RESULT(parse_text("[1,2,3,4", MAELYS_JSON_PROFILE_RFC8259, &limits,
        &document, &error), MAELYS_JSON_ERR_LIMIT);
    CHECK(error.offset == 7u);
    CHECK_RESULT(parse_text("[1,2,3,4]", MAELYS_JSON_PROFILE_RFC8259, &limits,
        &document, &error), MAELYS_JSON_ERR_LIMIT);
    CHECK(error.offset == 0u && error.line == 1u && error.column == 1u);
    limits.maximum_bytes = 9u;
    CHECK_RESULT(parse_text("[1,2,3,4]", MAELYS_JSON_PROFILE_RFC8259, &limits,
        &document, &error), MAELYS_JSON_ERR_LIMIT);
    limits.maximum_tokens = 5u;
    CHECK_RESULT(parse_text("[1,2,3,4]", MAELYS_JSON_PROFILE_RFC8259, &limits,
        &document, &error), MAELYS_JSON_OK);
    maelys_json_document_release(document);
    return 0;
}

static int utf8_table(void) {
    static const char *const invalid[] = {
        "\"\xc0\x80\"", "\"\xe0\x80\x80\"", "\"\xf0\x80\x80\x80\"",
        "\"\xed\xa0\x80\"", "\"\xf4\x90\x80\x80\"", "\"\xf5\x80\x80\x80\"",
        "\"\xe2\x82\"", "\"\x80\"", "\"\xc2\"", ("\"\xc2" "a\""), "\"\xff\""
    };
    static const char *const valid[] = {
        "\"\xc2\xa9\"", "\"\xe2\x82\xac\"", "\"\xf0\x9f\x9a\x80\"",
        "\"\xef\xbf\xbe\"", "\"\xf4\x8f\xbf\xbf\"", "\"\xed\x9f\xbf\""
    };
    maelys_json_document_t *document = NULL;
    for (size_t i = 0u; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        CHECK_RESULT(parse_text(invalid[i], MAELYS_JSON_PROFILE_RFC8259, NULL,
            &document, NULL), MAELYS_JSON_ERR_UTF8);
    }
    for (size_t i = 0u; i < sizeof(valid) / sizeof(valid[0]); ++i) {
        CHECK_RESULT(parse_text(valid[i], MAELYS_JSON_PROFILE_RFC8259, NULL,
            &document, NULL), MAELYS_JSON_OK);
        maelys_json_document_release(document);
    }
    return 0;
}

static int escapes(void) {
    maelys_json_document_t *document = NULL;
    CHECK_RESULT(parse_text("\"\\\"\\\\\\/\\b\\f\\n\\r\\t\\u00E9\\u20ac\\uD83D\\uDE80\"",
        MAELYS_JSON_PROFILE_RFC8259, NULL, &document, NULL), MAELYS_JSON_OK);
    maelys_json_view_t view;
    CHECK_RESULT(maelys_json_value_string(document, 0u, &view), MAELYS_JSON_OK);
    CHECK_VIEW(view, "\"\\/\b\f\n\r\t\xc3\xa9\xe2\x82\xac\xf0\x9f\x9a\x80");
    maelys_json_document_release(document);
    static const char *const rejected[] = {
        "\"\\ud800\"", "\"\\udc00\"", "\"\\ud800\\u0041\"", "\"\\ud800\\udbff\"",
        "\"\\u0000\"", "\"\\x41\"", "\"\\u12\"", "\"\\u12g4\"", "\"\\a\"",
        "\"\\U0041\"", "\"abc\\\"", "\"\\'\""
    };
    for (size_t i = 0u; i < sizeof(rejected) / sizeof(rejected[0]); ++i) {
        CHECK_RESULT(parse_text(rejected[i], MAELYS_JSON_PROFILE_RFC8259, NULL,
            &document, NULL), MAELYS_JSON_ERR_SYNTAX);
    }
    return 0;
}

static int number_grammar(void) {
    static const char *const valid[] = {
        "0", "-0", "1", "-1", "1.5", "1e5", "1E+5", "-1.5e-3", "0.0", "123456789012345678901234567890"
    };
    static const char *const invalid[] = {
        "01", "+1", "1.", ".5", "1e", "-", "1e+", "0x1", "NaN", "Infinity",
        "-Infinity", "- 1", "1.5.2", "1e5e5", "--1", "0b1", "1_000"
    };
    maelys_json_document_t *document = NULL;
    for (size_t i = 0u; i < sizeof(valid) / sizeof(valid[0]); ++i) {
        CHECK_RESULT(parse_text(valid[i], MAELYS_JSON_PROFILE_RFC8259, NULL,
            &document, NULL), MAELYS_JSON_OK);
        CHECK(maelys_json_value_type(document, 0u) == MAELYS_JSON_TYPE_NUMBER);
        maelys_json_document_release(document);
    }
    for (size_t i = 0u; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        CHECK_RESULT(parse_text(invalid[i], MAELYS_JSON_PROFILE_RFC8259, NULL,
            &document, NULL), MAELYS_JSON_ERR_SYNTAX);
    }
    return 0;
}

static int literals(void) {
    maelys_json_document_t *document = NULL;
    CHECK_RESULT(parse_text("[true,false,null]", MAELYS_JSON_PROFILE_RFC8259,
        NULL, &document, NULL), MAELYS_JSON_OK);
    CHECK(maelys_json_value_type(document, 1u) == MAELYS_JSON_TYPE_BOOLEAN);
    CHECK(maelys_json_value_type(document, 2u) == MAELYS_JSON_TYPE_BOOLEAN);
    CHECK(maelys_json_value_is_null(document, 3u));
    maelys_json_document_release(document);
    static const char *const rejected[] = {
        "truex", "nul", "True", "NULL", "[true false]", "t", "fals"
    };
    for (size_t i = 0u; i < sizeof(rejected) / sizeof(rejected[0]); ++i) {
        CHECK_RESULT(parse_text(rejected[i], MAELYS_JSON_PROFILE_RFC8259, NULL,
            &document, NULL), MAELYS_JSON_ERR_SYNTAX);
    }
    return 0;
}

static int argument_errors(void) {
    maelys_json_document_t *document = (maelys_json_document_t *)1;
    maelys_json_error_t error = {.code = MAELYS_JSON_ERR_RANGE};
    CHECK_RESULT(maelys_json_document_parse("1", 1u, (maelys_json_profile_t)0,
        NULL, &document, &error), MAELYS_JSON_ERR_ARGUMENT);
    CHECK(error.code == MAELYS_JSON_ERR_ARGUMENT && error.line == 1u);
    CHECK_RESULT(maelys_json_document_parse("1", 1u,
        MAELYS_JSON_PROFILE_RFC8259, NULL, NULL, &error),
        MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_document_parse(NULL, 1u,
        MAELYS_JSON_PROFILE_RFC8259, NULL, &document, &error),
        MAELYS_JSON_ERR_ARGUMENT);
    maelys_json_limits_t limits = {.maximum_bytes = SIZE_MAX};
    CHECK_RESULT(maelys_json_document_parse("1", 1u,
        MAELYS_JSON_PROFILE_RFC8259, &limits, &document, &error),
        MAELYS_JSON_ERR_ARGUMENT);
    limits.maximum_bytes = SIZE_MAX / 2u;
    CHECK_RESULT(maelys_json_document_parse("1", 1u,
        MAELYS_JSON_PROFILE_RFC8259, &limits, &document, &error),
        MAELYS_JSON_ERR_ARGUMENT);
    limits.maximum_bytes = SIZE_MAX / 2u - 1u;
    CHECK_RESULT(maelys_json_document_parse("1", 1u,
        MAELYS_JSON_PROFILE_RFC8259, &limits, &document, &error),
        MAELYS_JSON_OK);
    maelys_json_document_release(document);
    CHECK_RESULT(maelys_json_document_parse_file(NULL,
        MAELYS_JSON_PROFILE_RFC8259, NULL, &document, &error),
        MAELYS_JSON_ERR_ARGUMENT);
    CHECK(error.code == MAELYS_JSON_ERR_ARGUMENT);
    return 0;
}

static int duplicate_keys(void) {
    maelys_json_document_t *document = NULL;
    maelys_json_error_t error;
    CHECK_RESULT(parse_text("{\"a\":1,\"a\":2}", MAELYS_JSON_PROFILE_RFC8259,
        NULL, &document, &error), MAELYS_JSON_ERR_DUPLICATE_KEY);
    CHECK(error.offset == 7u);
    CHECK_RESULT(parse_text("{\"a\":1,\"\\u0061\":2}",
        MAELYS_JSON_PROFILE_RFC8259, NULL, &document, &error),
        MAELYS_JSON_ERR_DUPLICATE_KEY);
    CHECK_RESULT(parse_text("{\"\":1,\"\":2}", MAELYS_JSON_PROFILE_RFC8259,
        NULL, &document, &error), MAELYS_JSON_ERR_DUPLICATE_KEY);
    CHECK_RESULT(parse_text("[{\"a\":1},{\"a\":2},{\"a\":{\"a\":3}}]",
        MAELYS_JSON_PROFILE_RFC8259, NULL, &document, &error), MAELYS_JSON_OK);
    maelys_json_document_release(document);
    CHECK_RESULT(parse_text("{\"a\":1,\"a\":2}",
        MAELYS_JSON_PROFILE_CONTRACT_ASCII, NULL, &document, &error),
        MAELYS_JSON_ERR_DUPLICATE_KEY);
    return 0;
}

static int many_keys(void) {
    /* Exercises key-set growth well past its initial capacity. */
    char buffer[8192];
    size_t size = 0u;
    buffer[size++] = '{';
    for (int i = 0; i < 500; ++i) {
        size += (size_t)snprintf(buffer + size, sizeof(buffer) - size,
            "%s\"k%d\":%d", i ? "," : "", i, i);
    }
    buffer[size++] = '}';
    buffer[size] = '\0';
    maelys_json_document_t *document = NULL;
    CHECK_RESULT(parse_text(buffer, MAELYS_JSON_PROFILE_RFC8259, NULL,
        &document, NULL), MAELYS_JSON_OK);
    size_t count;
    CHECK_RESULT(maelys_json_object_size(document, 0u, &count), MAELYS_JSON_OK);
    CHECK(count == 500u);
    uint64_t number;
    CHECK_RESULT(maelys_json_object_get_u64(document, 0u, "k499", &number),
        MAELYS_JSON_OK);
    CHECK(number == 499u);
    maelys_json_document_release(document);
    return 0;
}

static int error_format(void) {
    maelys_json_error_t error = {
        .code = MAELYS_JSON_ERR_DUPLICATE_KEY, .offset = 12u, .line = 3u,
        .column = 4u
    };
    char buffer[64];
    size_t length = maelys_json_error_format(&error, buffer, sizeof(buffer));
    CHECK(strcmp(buffer, "line 3, column 4 (offset 12): duplicate object key") == 0);
    CHECK(length == strlen(buffer));
    char small[8];
    CHECK(maelys_json_error_format(&error, small, sizeof(small)) == length);
    CHECK(strcmp(small, "line 3,") == 0);
    CHECK(maelys_json_error_format(&error, NULL, 0u) == length);
    CHECK(maelys_json_error_format(NULL, buffer, sizeof(buffer)) == 0u);
    CHECK(strcmp(maelys_json_result_string((maelys_json_result_t)99),
        "unknown JSON error") == 0);
    return 0;
}

typedef struct pointer_case {
    const char *input;
    const char *pointer;
} pointer_case_t;

static int error_pointer(void) {
    static const pointer_case_t cases[] = {
        {"{\"commands\":[{}, {\"payload\": 1x}]}", "/commands/1/payload"},
        {"{\"a\":[1,2,3,tru]}", "/a/3"},
        {"{\"a\":{\"b\":{\"c\":\"unterminated", "/a/b/c"},
        {"{\"a\":1,\"a\":2}", ""},
        {"{\"a\":1, \"b\" 2}", ""},
        {"{\"a\":1, \"b\":{\"x\":1, \"y\" 2}}", "/b"},
        {"[[[]],[[],[1,[2,x]]]]", "/1/1/1/1"},
        {"{\"a/b\":{\"c~d\":[x]}}", "/a~1b/c~0d/0"},
        {"{\"\\u00e9\":[x]}", "/\xc3\xa9/0"},
        {"x", ""},
        {"", ""},
        {"{\"a\":[1,2],\"b\":x}", "/b"},
        {"[\"ab\xc0\x80\"]", "/0"},
    };
    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        maelys_json_document_t *document = NULL;
        maelys_json_error_t error;
        maelys_json_result_t result = parse_text(cases[i].input,
            MAELYS_JSON_PROFILE_RFC8259, NULL, &document, &error);
        char pointer[128];
        size_t length = maelys_json_error_pointer(cases[i].input,
            strlen(cases[i].input), &error, pointer, sizeof(pointer));
        if (result == MAELYS_JSON_OK || strcmp(pointer, cases[i].pointer) != 0 ||
            length != strlen(cases[i].pointer)) {
            fprintf(stderr, "    case %zu: %s -> \"%s\" (len %zu), expected \"%s\"\n",
                i, maelys_json_result_string(result), pointer, length,
                cases[i].pointer);
            maelys_json_document_release(document);
            return 1;
        }
    }
    /* Truncation and degenerate arguments behave like snprintf. */
    /* Offset 29 is the "1x" primitive: inside the value of "payload". */
    maelys_json_error_t error = {.code = MAELYS_JSON_ERR_SYNTAX, .offset = 29u};
    const char *input = "{\"commands\":[{}, {\"payload\": 1x}]}";
    char small[6];
    CHECK(maelys_json_error_pointer(input, strlen(input), &error, small,
        sizeof(small)) == strlen("/commands/1/payload"));
    CHECK(strcmp(small, "/comm") == 0);
    CHECK(maelys_json_error_pointer(input, strlen(input), &error, NULL, 0u) ==
        strlen("/commands/1/payload"));
    CHECK(maelys_json_error_pointer(input, strlen(input), NULL, small,
        sizeof(small)) == 0u && small[0] == '\0');
    CHECK(maelys_json_error_pointer(NULL, 3u, &error, small, sizeof(small)) == 0u);
    error.offset = 9999u;
    CHECK(maelys_json_error_pointer(input, strlen(input), &error, small,
        sizeof(small)) == 0u);
    return 0;
}

static int parse_file(void) {
    char path[512];
    snprintf(path, sizeof(path), "%s/08-contract-ascii.json",
        test_vectors_directory());
    maelys_json_document_t *document = NULL;
    maelys_json_error_t error;
    CHECK_RESULT(maelys_json_document_parse_file(path,
        MAELYS_JSON_PROFILE_CONTRACT_ASCII, NULL, &document, &error),
        MAELYS_JSON_OK);
    maelys_json_view_t view;
    CHECK_RESULT(maelys_json_object_get_string(document, 0u, "schema", &view),
        MAELYS_JSON_OK);
    CHECK_VIEW(view, "driver/v2");
    maelys_json_document_release(document);
    maelys_json_limits_t limits = {.maximum_bytes = 4u};
    CHECK_RESULT(maelys_json_document_parse_file(path,
        MAELYS_JSON_PROFILE_RFC8259, &limits, &document, &error),
        MAELYS_JSON_ERR_LIMIT);
    CHECK(error.code == MAELYS_JSON_ERR_LIMIT);
    CHECK_RESULT(maelys_json_document_parse_file("tests/vectors/missing.json",
        MAELYS_JSON_PROFILE_RFC8259, NULL, &document, &error),
        MAELYS_JSON_ERR_IO);
    CHECK(error.code == MAELYS_JSON_ERR_IO && document == NULL);
    CHECK_RESULT(maelys_json_document_parse_file(NULL,
        MAELYS_JSON_PROFILE_RFC8259, NULL, &document, &error),
        MAELYS_JSON_ERR_ARGUMENT);
    return 0;
}

static const test_case_t cases[] = {
    {"rfc_document", rfc_document},
    {"contract_profile", contract_profile},
    {"empty_and_whitespace", empty_and_whitespace},
    {"trailing_data", trailing_data},
    {"error_positions", error_positions},
    {"depth_semantics", depth_semantics},
    {"token_and_byte_limits", token_and_byte_limits},
    {"utf8_table", utf8_table},
    {"escapes", escapes},
    {"number_grammar", number_grammar},
    {"literals", literals},
    {"argument_errors", argument_errors},
    {"duplicate_keys", duplicate_keys},
    {"many_keys", many_keys},
    {"error_format", error_format},
    {"error_pointer", error_pointer},
    {"parse_file", parse_file},
};

TEST_SUITE(test_parser_suite, cases)

#include "framework.h"

#include <stdlib.h>

static int integers(void) {
    static const char input[] =
        "{\"umax\":18446744073709551615,\"uover\":18446744073709551616,"
        "\"imin\":-9223372036854775808,\"iunder\":-9223372036854775809,"
        "\"imax\":9223372036854775807,\"iover\":9223372036854775808,"
        "\"negzero\":-0,\"fraction\":1.5,\"exponent\":1e2,\"neg\":-42}";
    maelys_json_document_t *document = NULL;
    CHECK_RESULT(parse_text(input, MAELYS_JSON_PROFILE_RFC8259, NULL,
        &document, NULL), MAELYS_JSON_OK);
    uint64_t u;
    int64_t i;
    CHECK_RESULT(maelys_json_object_get_u64(document, 0u, "umax", &u), MAELYS_JSON_OK);
    CHECK(u == UINT64_MAX);
    CHECK_RESULT(maelys_json_object_get_u64(document, 0u, "uover", &u), MAELYS_JSON_ERR_RANGE);
    CHECK_RESULT(maelys_json_object_get_i64(document, 0u, "umax", &i), MAELYS_JSON_ERR_RANGE);
    CHECK_RESULT(maelys_json_object_get_i64(document, 0u, "imin", &i), MAELYS_JSON_OK);
    CHECK(i == INT64_MIN);
    CHECK_RESULT(maelys_json_object_get_i64(document, 0u, "iunder", &i), MAELYS_JSON_ERR_RANGE);
    CHECK_RESULT(maelys_json_object_get_i64(document, 0u, "imax", &i), MAELYS_JSON_OK);
    CHECK(i == INT64_MAX);
    CHECK_RESULT(maelys_json_object_get_i64(document, 0u, "iover", &i), MAELYS_JSON_ERR_RANGE);
    CHECK_RESULT(maelys_json_object_get_u64(document, 0u, "iover", &u), MAELYS_JSON_OK);
    CHECK(u == UINT64_C(9223372036854775808));
    CHECK_RESULT(maelys_json_object_get_u64(document, 0u, "negzero", &u), MAELYS_JSON_OK);
    CHECK(u == 0u);
    CHECK_RESULT(maelys_json_object_get_i64(document, 0u, "negzero", &i), MAELYS_JSON_OK);
    CHECK(i == 0);
    CHECK_RESULT(maelys_json_object_get_u64(document, 0u, "neg", &u), MAELYS_JSON_ERR_RANGE);
    CHECK_RESULT(maelys_json_object_get_i64(document, 0u, "neg", &i), MAELYS_JSON_OK);
    CHECK(i == -42);
    CHECK_RESULT(maelys_json_object_get_u64(document, 0u, "fraction", &u), MAELYS_JSON_ERR_NOT_INTEGER);
    CHECK_RESULT(maelys_json_object_get_i64(document, 0u, "fraction", &i), MAELYS_JSON_ERR_NOT_INTEGER);
    CHECK_RESULT(maelys_json_object_get_i64(document, 0u, "exponent", &i), MAELYS_JSON_ERR_NOT_INTEGER);
    maelys_json_value_t value;
    maelys_json_view_t view;
    CHECK_RESULT(maelys_json_object_get(document, 0u, "fraction", &value), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_value_number_text(document, value, &view), MAELYS_JSON_OK);
    CHECK_VIEW(view, "1.5");
    CHECK(strtod(view.data, NULL) == 1.5);
    CHECK_RESULT(maelys_json_value_string(document, value, &view), MAELYS_JSON_ERR_TYPE);
    CHECK_RESULT(maelys_json_object_get(document, 0u, "neg", &value), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_value_number_text(document, value, &view), MAELYS_JSON_OK);
    CHECK_VIEW(view, "-42");
    CHECK_RESULT(maelys_json_value_number_text(document, 0u, &view), MAELYS_JSON_ERR_TYPE);
    maelys_json_document_release(document);
    return 0;
}

static int object_access(void) {
    maelys_json_document_t *document = NULL;
    CHECK_RESULT(parse_text("{\"b\":1,\"a\":[],\"\":true,\"ab\":\"x\"}",
        MAELYS_JSON_PROFILE_RFC8259, NULL, &document, NULL), MAELYS_JSON_OK);
    size_t count;
    CHECK_RESULT(maelys_json_object_size(document, 0u, &count), MAELYS_JSON_OK);
    CHECK(count == 4u);
    maelys_json_value_t value;
    CHECK_RESULT(maelys_json_object_get(document, 0u, "a", &value), MAELYS_JSON_OK);
    CHECK(maelys_json_value_type(document, value) == MAELYS_JSON_TYPE_ARRAY);
    CHECK_RESULT(maelys_json_object_get_sized(document, 0u, "ab", 1u, &value), MAELYS_JSON_OK);
    CHECK(maelys_json_value_type(document, value) == MAELYS_JSON_TYPE_ARRAY);
    CHECK_RESULT(maelys_json_object_get_sized(document, 0u, "abc", 2u, &value), MAELYS_JSON_OK);
    CHECK(maelys_json_value_type(document, value) == MAELYS_JSON_TYPE_STRING);
    CHECK_RESULT(maelys_json_object_get(document, 0u, "", &value), MAELYS_JSON_OK);
    CHECK(maelys_json_value_type(document, value) == MAELYS_JSON_TYPE_BOOLEAN);
    CHECK_RESULT(maelys_json_object_get_sized(document, 0u, NULL, 0u, &value), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_object_get(document, 0u, "zz", &value), MAELYS_JSON_ERR_NOT_FOUND);
    CHECK_RESULT(maelys_json_object_get(document, 0u, NULL, &value), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_object_get(document, 0u, "a", NULL), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_object_get(NULL, 0u, "a", &value), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_object_get(document, 99u, "a", &value), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_object_get(document, MAELYS_JSON_VALUE_NONE, "a", &value), MAELYS_JSON_ERR_ARGUMENT);
    maelys_json_view_t key;
    CHECK_RESULT(maelys_json_object_member_at(document, 0u, 0u, &key, &value), MAELYS_JSON_OK);
    CHECK_VIEW(key, "b");
    CHECK_RESULT(maelys_json_object_member_at(document, 0u, 3u, &key, &value), MAELYS_JSON_OK);
    CHECK_VIEW(key, "ab");
    CHECK(maelys_json_value_type(document, value) == MAELYS_JSON_TYPE_STRING);
    CHECK_RESULT(maelys_json_object_member_at(document, 0u, 2u, NULL, &value), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_object_member_at(document, 0u, 4u, &key, &value), MAELYS_JSON_ERR_RANGE);
    maelys_json_value_t array;
    CHECK_RESULT(maelys_json_object_get(document, 0u, "a", &array), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_object_size(document, array, &count), MAELYS_JSON_ERR_TYPE);
    CHECK_RESULT(maelys_json_object_get(document, array, "a", &value), MAELYS_JSON_ERR_TYPE);
    CHECK_RESULT(maelys_json_array_size(document, 0u, &count), MAELYS_JSON_ERR_TYPE);
    maelys_json_document_release(document);
    return 0;
}

static int array_access(void) {
    char buffer[8192];
    size_t size = 0u;
    buffer[size++] = '[';
    for (int i = 0; i < 1000; ++i) {
        size += (size_t)snprintf(buffer + size, sizeof(buffer) - size, "%s%d",
            i ? "," : "", i);
    }
    buffer[size++] = ']';
    buffer[size] = '\0';
    maelys_json_document_t *document = NULL;
    CHECK_RESULT(parse_text(buffer, MAELYS_JSON_PROFILE_RFC8259, NULL,
        &document, NULL), MAELYS_JSON_OK);
    size_t count;
    CHECK_RESULT(maelys_json_array_size(document, 0u, &count), MAELYS_JSON_OK);
    CHECK(count == 1000u);
    for (size_t i = 0u; i < count; ++i) {
        maelys_json_value_t element;
        uint64_t number;
        CHECK_RESULT(maelys_json_array_get(document, 0u, i, &element), MAELYS_JSON_OK);
        CHECK_RESULT(maelys_json_value_u64(document, element, &number), MAELYS_JSON_OK);
        CHECK(number == i);
    }
    maelys_json_value_t element;
    CHECK_RESULT(maelys_json_array_get(document, 0u, 1000u, &element), MAELYS_JSON_ERR_RANGE);
    CHECK_RESULT(maelys_json_array_get(document, 1u, 0u, &element), MAELYS_JSON_ERR_TYPE);
    CHECK_RESULT(maelys_json_array_get(document, 0u, 0u, NULL), MAELYS_JSON_ERR_ARGUMENT);
    maelys_json_document_release(document);
    CHECK_RESULT(parse_text("[]", MAELYS_JSON_PROFILE_RFC8259, NULL,
        &document, NULL), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_array_size(document, 0u, &count), MAELYS_JSON_OK);
    CHECK(count == 0u);
    CHECK_RESULT(maelys_json_array_get(document, 0u, 0u, &element), MAELYS_JSON_ERR_RANGE);
    maelys_json_document_release(document);
    return 0;
}

static int nested_containers(void) {
    maelys_json_document_t *document = NULL;
    CHECK_RESULT(parse_text("{\"a\":[{\"b\":[1,[2,3]]},{}],\"c\":{\"d\":{}}}",
        MAELYS_JSON_PROFILE_RFC8259, NULL, &document, NULL), MAELYS_JSON_OK);
    maelys_json_value_t a, first, b, inner, three;
    CHECK_RESULT(maelys_json_object_get(document, 0u, "a", &a), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_array_get(document, a, 0u, &first), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_object_get(document, first, "b", &b), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_array_get(document, b, 1u, &inner), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_array_get(document, inner, 1u, &three), MAELYS_JSON_OK);
    uint64_t number;
    CHECK_RESULT(maelys_json_value_u64(document, three, &number), MAELYS_JSON_OK);
    CHECK(number == 3u);
    size_t count;
    CHECK_RESULT(maelys_json_array_get(document, a, 1u, &first), MAELYS_JSON_OK);
    CHECK_RESULT(maelys_json_object_size(document, first, &count), MAELYS_JSON_OK);
    CHECK(count == 0u);
    maelys_json_document_release(document);
    return 0;
}

static int helpers(void) {
    maelys_json_document_t *document = NULL;
    CHECK_RESULT(parse_text("{\"s\":\"text\",\"u\":7,\"i\":-7,\"b\":true,\"n\":null}",
        MAELYS_JSON_PROFILE_RFC8259, NULL, &document, NULL), MAELYS_JSON_OK);
    maelys_json_view_t view;
    uint64_t u;
    int64_t i;
    int enabled;
    CHECK_RESULT(maelys_json_object_get_string(document, 0u, "s", &view), MAELYS_JSON_OK);
    CHECK_VIEW(view, "text");
    CHECK_RESULT(maelys_json_object_get_u64(document, 0u, "u", &u), MAELYS_JSON_OK);
    CHECK(u == 7u);
    CHECK_RESULT(maelys_json_object_get_i64(document, 0u, "i", &i), MAELYS_JSON_OK);
    CHECK(i == -7);
    CHECK_RESULT(maelys_json_object_get_boolean(document, 0u, "b", &enabled), MAELYS_JSON_OK);
    CHECK(enabled == 1);
    CHECK_RESULT(maelys_json_object_get_string(document, 0u, "u", &view), MAELYS_JSON_ERR_TYPE);
    CHECK_RESULT(maelys_json_object_get_boolean(document, 0u, "n", &enabled), MAELYS_JSON_ERR_TYPE);
    CHECK_RESULT(maelys_json_object_get_u64(document, 0u, "missing", &u), MAELYS_JSON_ERR_NOT_FOUND);
    CHECK_RESULT(maelys_json_object_get_string(document, 0u, "s", NULL), MAELYS_JSON_ERR_ARGUMENT);
    maelys_json_value_t n;
    CHECK_RESULT(maelys_json_object_get(document, 0u, "n", &n), MAELYS_JSON_OK);
    CHECK(maelys_json_value_is_null(document, n));
    CHECK(!maelys_json_value_is_null(document, 0u));
    CHECK_RESULT(maelys_json_value_boolean(document, n, &enabled), MAELYS_JSON_ERR_TYPE);
    maelys_json_document_release(document);
    return 0;
}

static int invalid_handles(void) {
    maelys_json_document_t *document = NULL;
    CHECK_RESULT(parse_text("[1]", MAELYS_JSON_PROFILE_RFC8259, NULL,
        &document, NULL), MAELYS_JSON_OK);
    CHECK(maelys_json_value_type(document, 9999u) == MAELYS_JSON_TYPE_NONE);
    CHECK(maelys_json_value_type(document, MAELYS_JSON_VALUE_NONE) == MAELYS_JSON_TYPE_NONE);
    CHECK(maelys_json_value_type(NULL, 0u) == MAELYS_JSON_TYPE_NONE);
    CHECK(maelys_json_document_root(NULL) == MAELYS_JSON_VALUE_NONE);
    CHECK(!maelys_json_value_is_null(NULL, 0u));
    CHECK(!maelys_json_value_is_null(document, 5u));
    uint64_t u;
    CHECK_RESULT(maelys_json_value_u64(NULL, 0u, &u), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_value_u64(document, 1u, NULL), MAELYS_JSON_ERR_ARGUMENT);
    CHECK_RESULT(maelys_json_value_u64(document, 7u, &u), MAELYS_JSON_ERR_ARGUMENT);
    maelys_json_document_release(document);
    maelys_json_document_release(NULL);
    return 0;
}

static int version(void) {
    CHECK(strcmp(maelys_json_version(), MAELYS_JSON_VERSION_STRING) == 0);
    char expected[32];
    snprintf(expected, sizeof(expected), "%d.%d.%d", MAELYS_JSON_VERSION_MAJOR,
        MAELYS_JSON_VERSION_MINOR, MAELYS_JSON_VERSION_PATCH);
    CHECK(strcmp(expected, MAELYS_JSON_VERSION_STRING) == 0);
    return 0;
}

static const test_case_t cases[] = {
    {"integers", integers},
    {"object_access", object_access},
    {"array_access", array_access},
    {"nested_containers", nested_containers},
    {"helpers", helpers},
    {"invalid_handles", invalid_handles},
    {"version", version},
};

TEST_SUITE(test_reader_suite, cases)

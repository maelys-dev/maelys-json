/* SPDX-License-Identifier: MPL-2.0 */
/*
 * Golden vectors for Maelys Canonical JSON v1. Each tests/vectors/NAME.json
 * input must canonicalize to exactly NAME.canonical, and the canonical bytes
 * must be a fixed point. Optional NAME.indented and NAME.ascii files pin the
 * presentation forms. Changing any expected file changes the bytes that
 * signatures are computed over: treat it as a format change.
 */
#include "framework.h"

#include <stdlib.h>

static const char *const vector_names[] = {
    "01-scalars", "02-nested", "03-key-order", "04-unicode", "05-escapes",
    "06-integers", "07-empty-containers", "08-contract-ascii",
    "09-key-order-utf16"
};

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
            capacity = capacity ? capacity * 2u : 1024u;
            char *grown = realloc(buffer, capacity + 1u);
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
    buffer[size] = '\0';
    *out_size = size;
    return buffer;
}

static char *read_vector(const char *name, const char *extension, size_t *out_size) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.%s", test_vectors_directory(), name,
        extension);
    return read_file(path, out_size);
}

/* Parses `input` and serializes it with `flags`; returns the output. */
static char *canonicalize(const char *input, size_t size, unsigned int flags,
    size_t *out_size, const char *name) {
    maelys_json_document_t *document = NULL;
    maelys_json_error_t error;
    maelys_json_result_t result = maelys_json_document_parse(input, size,
        MAELYS_JSON_PROFILE_RFC8259, NULL, &document, &error);
    if (result != MAELYS_JSON_OK) {
        char message[128];
        maelys_json_error_format(&error, message, sizeof(message));
        fprintf(stderr, "    %s: parse failed: %s\n", name, message);
        return NULL;
    }
    maelys_json_writer_t *writer = NULL;
    char *output = NULL;
    if (maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, NULL, flags,
            &writer) == MAELYS_JSON_OK &&
        maelys_json_writer_value(writer, document, 0u) == MAELYS_JSON_OK) {
        result = maelys_json_writer_finish(writer, &output, out_size);
    } else {
        result = writer ? maelys_json_writer_status(writer) : MAELYS_JSON_ERR_MEMORY;
    }
    if (result != MAELYS_JSON_OK) {
        fprintf(stderr, "    %s: writer failed: %s\n", name,
            maelys_json_result_string(result));
    }
    maelys_json_writer_release(writer);
    maelys_json_document_release(document);
    return output;
}

static int compare(const char *name, const char *form, const char *actual,
    size_t actual_size, const char *expected, size_t expected_size) {
    if (actual_size == expected_size &&
        memcmp(actual, expected, expected_size) == 0) {
        return 0;
    }
    fprintf(stderr, "    %s (%s) mismatch\n    expected: %s\n    got:      %s\n",
        name, form, expected, actual);
    return 1;
}

static int check_form(const char *name, const char *input, size_t input_size,
    const char *extension, unsigned int flags, int required) {
    size_t expected_size;
    char *expected = read_vector(name, extension, &expected_size);
    if (!expected) {
        if (required) {
            fprintf(stderr, "    %s: missing %s file\n", name, extension);
        }
        return required;
    }
    size_t actual_size;
    char *actual = canonicalize(input, input_size, flags, &actual_size, name);
    int failed = !actual || compare(name, extension, actual, actual_size,
        expected, expected_size);
    /* Idempotence: the canonical bytes canonicalize to themselves, and
     * maelys_json_document_is_canonical agrees. */
    if (!failed && !flags) {
        size_t again_size;
        char *again = canonicalize(expected, expected_size, 0u, &again_size, name);
        failed = !again || compare(name, "fixed point", again, again_size,
            expected, expected_size);
        free(again);
        maelys_json_document_t *document = NULL;
        int canonical = 0;
        if (!failed && (maelys_json_document_parse(expected, expected_size,
                MAELYS_JSON_PROFILE_RFC8259, NULL, &document, NULL) != MAELYS_JSON_OK ||
            maelys_json_document_is_canonical(document, 0u, &canonical) != MAELYS_JSON_OK ||
            !canonical)) {
            fprintf(stderr, "    %s: is_canonical disagrees\n", name);
            failed = 1;
        }
        maelys_json_document_release(document);
    }
    free(actual);
    free(expected);
    return failed;
}

static int run_vector(const char *name) {
    size_t input_size;
    char *input = read_vector(name, "json", &input_size);
    if (!input) {
        fprintf(stderr, "    %s: missing input\n", name);
        return 1;
    }
    int failed = check_form(name, input, input_size, "canonical", 0u, 1) ||
        check_form(name, input, input_size, "indented",
            MAELYS_JSON_WRITER_INDENT | MAELYS_JSON_WRITER_FINAL_NEWLINE, 0) ||
        check_form(name, input, input_size, "ascii", MAELYS_JSON_WRITER_ASCII, 0);
    free(input);
    return failed;
}

static int all_vectors(void) {
    int failed = 0;
    for (size_t i = 0u; i < sizeof(vector_names) / sizeof(vector_names[0]); ++i) {
        failed |= run_vector(vector_names[i]);
    }
    return failed;
}

static int contract_vector_parses_strictly(void) {
    size_t size;
    char *input = read_vector("08-contract-ascii", "canonical", &size);
    CHECK(input != NULL);
    maelys_json_document_t *document = NULL;
    maelys_json_result_t result = maelys_json_document_parse(input, size,
        MAELYS_JSON_PROFILE_CONTRACT_ASCII, NULL, &document, NULL);
    free(input);
    maelys_json_document_release(document);
    CHECK_RESULT(result, MAELYS_JSON_OK);
    return 0;
}

static const test_case_t cases[] = {
    {"all_vectors", all_vectors},
    {"contract_vector_parses_strictly", contract_vector_parses_strictly},
};

TEST_SUITE(test_vectors_suite, cases)

/* SPDX-License-Identifier: MPL-2.0 */
/*
 * The canonical check as maelys-json implemented it until the streaming
 * version: copy the document into a writer, serialize, compare the bytes,
 * with a prior walk ruling out a number the canonical numeric domain does
 * not hold. Written with the public API alone so that the test suite and
 * the fuzz harness can both hold the current implementation against it.
 */
#ifndef MAELYS_JSON_TEST_REFERENCE_CANONICAL_H
#define MAELYS_JSON_TEST_REFERENCE_CANONICAL_H

#include "maelys/json.h"

#include <stdlib.h>
#include <string.h>

/* 1 when some number of the subtree has a fraction or an exponent. */
static inline int holds_non_integral_number(
    const maelys_json_document_t *document, maelys_json_value_t value) {
    maelys_json_type_t type = maelys_json_value_type(document, value);
    if (type == MAELYS_JSON_TYPE_NUMBER) {
        maelys_json_view_t text;
        if (maelys_json_value_number_text(document, value, &text) != MAELYS_JSON_OK) {
            return 0;
        }
        return strcspn(text.data, ".eE") != text.size;
    }
    if (type == MAELYS_JSON_TYPE_ARRAY) {
        size_t count = 0u;
        if (maelys_json_array_size(document, value, &count) != MAELYS_JSON_OK) {
            return 0;
        }
        for (size_t i = 0u; i < count; ++i) {
            maelys_json_value_t element;
            if (maelys_json_array_get(document, value, i, &element) == MAELYS_JSON_OK &&
                holds_non_integral_number(document, element)) {
                return 1;
            }
        }
        return 0;
    }
    if (type == MAELYS_JSON_TYPE_OBJECT) {
        size_t count = 0u;
        if (maelys_json_object_size(document, value, &count) != MAELYS_JSON_OK) {
            return 0;
        }
        for (size_t i = 0u; i < count; ++i) {
            maelys_json_value_t member;
            if (maelys_json_object_member_at(document, value, i, NULL, &member) ==
                    MAELYS_JSON_OK && holds_non_integral_number(document, member)) {
                return 1;
            }
        }
        return 0;
    }
    return 0;
}

/* The replaced implementation, in the public API. `text` and `size` are the
 * bytes the document was parsed from, which it read internally. */
static inline maelys_json_result_t reference_is_canonical(
    const maelys_json_document_t *document, const char *text, size_t size,
    maelys_json_profile_t profile, unsigned int flags, int *out_canonical) {
    if (!document || !out_canonical ||
        (flags & ~MAELYS_JSON_WRITER_FINAL_NEWLINE)) {
        return MAELYS_JSON_ERR_ARGUMENT;
    }
    if (holds_non_integral_number(document, maelys_json_document_root(document))) {
        *out_canonical = 0;
        return MAELYS_JSON_OK;
    }
    /* The canonical form is never longer than the input, and a token never
     * yields more than one node, so these bounds only cut off output that
     * could not have matched. */
    maelys_json_limits_t limits = {
        .maximum_bytes = size, .maximum_depth = MAELYS_JSON_MAXIMUM_DEPTH,
        .maximum_tokens = size + 1u
    };
    maelys_json_writer_t *writer = NULL;
    maelys_json_result_t result = maelys_json_writer_create(profile, &limits,
        flags, &writer);
    if (result != MAELYS_JSON_OK) {
        return result;
    }
    char *bytes = NULL;
    size_t written = 0u;
    result = maelys_json_writer_value(writer, document,
        maelys_json_document_root(document));
    if (result == MAELYS_JSON_OK) {
        result = maelys_json_writer_finish(writer, &bytes, &written);
    }
    maelys_json_writer_release(writer);
    if (result == MAELYS_JSON_OK) {
        *out_canonical = written == size && memcmp(bytes, text, written) == 0;
        free(bytes);
        return MAELYS_JSON_OK;
    }
    if (result == MAELYS_JSON_ERR_RANGE || result == MAELYS_JSON_ERR_LIMIT) {
        *out_canonical = 0;
        return MAELYS_JSON_OK;
    }
    return result;
}

#endif

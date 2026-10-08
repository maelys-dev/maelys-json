/* SPDX-License-Identifier: MPL-2.0 */
#include "internal.h"

#include <stdlib.h>
#include <string.h>

maelys_json_result_t maelys_json_object_get_string(
    const maelys_json_document_t *document, maelys_json_value_t object,
    const char *key, maelys_json_view_t *out_view) {
    maelys_json_value_t value;
    maelys_json_result_t result = maelys_json_object_get(document, object, key,
        &value);
    return result == MAELYS_JSON_OK ?
        maelys_json_value_string(document, value, out_view) : result;
}

maelys_json_result_t maelys_json_object_get_u64(
    const maelys_json_document_t *document, maelys_json_value_t object,
    const char *key, uint64_t *out_number) {
    maelys_json_value_t value;
    maelys_json_result_t result = maelys_json_object_get(document, object, key,
        &value);
    return result == MAELYS_JSON_OK ?
        maelys_json_value_u64(document, value, out_number) : result;
}

maelys_json_result_t maelys_json_object_get_i64(
    const maelys_json_document_t *document, maelys_json_value_t object,
    const char *key, int64_t *out_number) {
    maelys_json_value_t value;
    maelys_json_result_t result = maelys_json_object_get(document, object, key,
        &value);
    return result == MAELYS_JSON_OK ?
        maelys_json_value_i64(document, value, out_number) : result;
}

maelys_json_result_t maelys_json_object_get_boolean(
    const maelys_json_document_t *document, maelys_json_value_t object,
    const char *key, int *out_enabled) {
    maelys_json_value_t value;
    maelys_json_result_t result = maelys_json_object_get(document, object, key,
        &value);
    return result == MAELYS_JSON_OK ?
        maelys_json_value_boolean(document, value, out_enabled) : result;
}

/* The canonical numeric domain is integral, while the writer can copy any
 * number by its lexeme: a document carrying a fraction or an exponent would
 * round-trip byte for byte without being canonical, so it is ruled out here
 * rather than by the comparison below. */
static int holds_non_integral_number(const maelys_json_document_t *document) {
    for (size_t i = 0u; i < document->token_count; ++i) {
        const maelys_json_token_t *token = &document->tokens[i];
        if (token->type != MAELYS_JSON_TYPE_NUMBER) {
            continue;
        }
        for (size_t j = 0u; j < token->text_size; ++j) {
            char byte = token->text[j];
            if (byte == '.' || byte == 'e' || byte == 'E') {
                return 1;
            }
        }
    }
    return 0;
}

maelys_json_result_t maelys_json_document_is_canonical(
    const maelys_json_document_t *document, unsigned int flags,
    int *out_canonical) {
    if (!document || !out_canonical ||
        (flags & ~MAELYS_JSON_WRITER_FINAL_NEWLINE)) {
        return MAELYS_JSON_ERR_ARGUMENT;
    }
    if (holds_non_integral_number(document)) {
        *out_canonical = 0;
        return MAELYS_JSON_OK;
    }
    /* The canonical form is never longer than the input, and never has more
     * nodes than the parse had tokens, so these limits only cut off output
     * that could not match anyway. */
    maelys_json_limits_t limits = {
        .maximum_bytes = document->size,
        .maximum_depth = MAELYS_JSON_MAXIMUM_DEPTH,
        .maximum_tokens = document->token_count
    };
    maelys_json_writer_t *writer = NULL;
    maelys_json_result_t result = maelys_json_writer_create(document->profile,
        &limits, flags, &writer);
    if (result != MAELYS_JSON_OK) {
        return result;
    }
    char *bytes = NULL;
    size_t size = 0u;
    result = maelys_json_writer_value(writer, document, 0u);
    if (result == MAELYS_JSON_OK) {
        result = maelys_json_writer_finish(writer, &bytes, &size);
    }
    maelys_json_writer_release(writer);
    switch (result) {
        case MAELYS_JSON_OK:
            *out_canonical = size == document->size &&
                memcmp(bytes, document->bytes, size) == 0;
            free(bytes);
            return MAELYS_JSON_OK;
        case MAELYS_JSON_ERR_NOT_INTEGER:
        case MAELYS_JSON_ERR_RANGE:
        case MAELYS_JSON_ERR_LIMIT:
            /* Not representable canonically, or longer than the input. The
             * writer no longer answers NOT_INTEGER; the enumerator stays so
             * this switch covers the result type. */
            *out_canonical = 0;
            return MAELYS_JSON_OK;
        case MAELYS_JSON_ERR_MEMORY:
            return result;
        case MAELYS_JSON_ERR_ARGUMENT:
        case MAELYS_JSON_ERR_SYNTAX:
        case MAELYS_JSON_ERR_UTF8:
        case MAELYS_JSON_ERR_DUPLICATE_KEY:
        case MAELYS_JSON_ERR_TYPE:
        case MAELYS_JSON_ERR_STATE:
        case MAELYS_JSON_ERR_NOT_FOUND:
        case MAELYS_JSON_ERR_IO:
            break;
    }
    /* A parsed document always copies into a writer of its own profile. */
    MAELYS_JSON_ASSERT(result == MAELYS_JSON_OK);
    return MAELYS_JSON_ERR_STATE;
}

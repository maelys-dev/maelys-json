#include "internal.h"

#include <stdlib.h>
#include <string.h>

void maelys_json_document_release(maelys_json_document_t *document) {
    if (!document) {
        return;
    }
    free(document->children);
    free(document->arena);
    free(document->tokens);
    free(document->bytes);
    free(document);
}

maelys_json_value_t maelys_json_document_root(
    const maelys_json_document_t *document) {
    return document && document->token_count ? 0u : MAELYS_JSON_VALUE_NONE;
}

maelys_json_type_t maelys_json_value_type(
    const maelys_json_document_t *document, maelys_json_value_t value) {
    return document && value < document->token_count ?
        document->tokens[value].type : MAELYS_JSON_TYPE_NONE;
}

/* Resolves a handle to a token of the expected type. */
static maelys_json_result_t token_of(
    const maelys_json_document_t *document, maelys_json_value_t value,
    maelys_json_type_t expected, const maelys_json_token_t **out_token) {
    if (!document || value >= document->token_count) {
        return MAELYS_JSON_ERR_ARGUMENT;
    }
    const maelys_json_token_t *token = &document->tokens[value];
    if (token->type != expected) {
        return MAELYS_JSON_ERR_TYPE;
    }
    *out_token = token;
    return MAELYS_JSON_OK;
}

static size_t child_at(
    const maelys_json_document_t *document, const maelys_json_token_t *token,
    size_t position) {
    MAELYS_JSON_ASSERT(position < token->links.child_count);
    return document->children[token->children_offset + position];
}

maelys_json_result_t maelys_json_object_get_sized(
    const maelys_json_document_t *document, maelys_json_value_t object,
    const char *key, size_t key_size, maelys_json_value_t *out_value) {
    if ((!key && key_size) || !out_value) {
        return MAELYS_JSON_ERR_ARGUMENT;
    }
    const maelys_json_token_t *container;
    maelys_json_result_t result = token_of(document, object,
        MAELYS_JSON_TYPE_OBJECT, &container);
    if (result != MAELYS_JSON_OK) {
        return result;
    }
    size_t members = container->links.child_count / 2u;
    for (size_t i = 0u; i < members; ++i) {
        const maelys_json_token_t *key_token =
            &document->tokens[child_at(document, container, 2u * i)];
        MAELYS_JSON_ASSERT(key_token->object_key);
        if (key_token->text_size == key_size &&
            (key_size == 0u || memcmp(key_token->text, key, key_size) == 0)) {
            *out_value = child_at(document, container, 2u * i + 1u);
            return MAELYS_JSON_OK;
        }
    }
    return MAELYS_JSON_ERR_NOT_FOUND;
}

maelys_json_result_t maelys_json_object_get(
    const maelys_json_document_t *document, maelys_json_value_t object,
    const char *key, maelys_json_value_t *out_value) {
    if (!key) {
        return MAELYS_JSON_ERR_ARGUMENT;
    }
    return maelys_json_object_get_sized(document, object, key, strlen(key),
        out_value);
}

maelys_json_result_t maelys_json_object_size(
    const maelys_json_document_t *document, maelys_json_value_t object,
    size_t *out_size) {
    if (!out_size) {
        return MAELYS_JSON_ERR_ARGUMENT;
    }
    const maelys_json_token_t *container;
    maelys_json_result_t result = token_of(document, object,
        MAELYS_JSON_TYPE_OBJECT, &container);
    if (result != MAELYS_JSON_OK) {
        return result;
    }
    MAELYS_JSON_ASSERT(container->links.child_count % 2u == 0u);
    *out_size = container->links.child_count / 2u;
    return MAELYS_JSON_OK;
}

maelys_json_result_t maelys_json_object_member_at(
    const maelys_json_document_t *document, maelys_json_value_t object,
    size_t index, maelys_json_view_t *out_key,
    maelys_json_value_t *out_value) {
    if (!out_value) {
        return MAELYS_JSON_ERR_ARGUMENT;
    }
    const maelys_json_token_t *container;
    maelys_json_result_t result = token_of(document, object,
        MAELYS_JSON_TYPE_OBJECT, &container);
    if (result != MAELYS_JSON_OK) {
        return result;
    }
    if (index >= container->links.child_count / 2u) {
        return MAELYS_JSON_ERR_RANGE;
    }
    const maelys_json_token_t *key_token =
        &document->tokens[child_at(document, container, 2u * index)];
    MAELYS_JSON_ASSERT(key_token->object_key);
    if (out_key) {
        *out_key = (maelys_json_view_t){
            .data = key_token->text, .size = key_token->text_size
        };
    }
    *out_value = child_at(document, container, 2u * index + 1u);
    return MAELYS_JSON_OK;
}

maelys_json_result_t maelys_json_array_size(
    const maelys_json_document_t *document, maelys_json_value_t array,
    size_t *out_size) {
    if (!out_size) {
        return MAELYS_JSON_ERR_ARGUMENT;
    }
    const maelys_json_token_t *container;
    maelys_json_result_t result = token_of(document, array,
        MAELYS_JSON_TYPE_ARRAY, &container);
    if (result != MAELYS_JSON_OK) {
        return result;
    }
    *out_size = container->links.child_count;
    return MAELYS_JSON_OK;
}

maelys_json_result_t maelys_json_array_get(
    const maelys_json_document_t *document, maelys_json_value_t array,
    size_t index, maelys_json_value_t *out_value) {
    if (!out_value) {
        return MAELYS_JSON_ERR_ARGUMENT;
    }
    const maelys_json_token_t *container;
    maelys_json_result_t result = token_of(document, array,
        MAELYS_JSON_TYPE_ARRAY, &container);
    if (result != MAELYS_JSON_OK) {
        return result;
    }
    if (index >= container->links.child_count) {
        return MAELYS_JSON_ERR_RANGE;
    }
    *out_value = child_at(document, container, index);
    return MAELYS_JSON_OK;
}

static maelys_json_result_t text_view(
    const maelys_json_document_t *document, maelys_json_value_t value,
    maelys_json_type_t expected, maelys_json_view_t *out_view) {
    if (!out_view) {
        return MAELYS_JSON_ERR_ARGUMENT;
    }
    const maelys_json_token_t *token;
    maelys_json_result_t result = token_of(document, value, expected, &token);
    if (result != MAELYS_JSON_OK) {
        return result;
    }
    *out_view = (maelys_json_view_t){
        .data = token->text, .size = token->text_size
    };
    return MAELYS_JSON_OK;
}

maelys_json_result_t maelys_json_value_string(
    const maelys_json_document_t *document, maelys_json_value_t value,
    maelys_json_view_t *out_view) {
    return text_view(document, value, MAELYS_JSON_TYPE_STRING, out_view);
}

maelys_json_result_t maelys_json_value_number_text(
    const maelys_json_document_t *document, maelys_json_value_t value,
    maelys_json_view_t *out_view) {
    return text_view(document, value, MAELYS_JSON_TYPE_NUMBER, out_view);
}

/* Reads an integer lexeme as a magnitude no larger than `maximum`. */
static maelys_json_result_t read_magnitude(
    const maelys_json_document_t *document, maelys_json_value_t value,
    uint64_t maximum, uint64_t *out_magnitude, int *out_negative) {
    const maelys_json_token_t *token;
    maelys_json_result_t result = token_of(document, value,
        MAELYS_JSON_TYPE_NUMBER, &token);
    if (result != MAELYS_JSON_OK) {
        return result;
    }
    const char *bytes = token->text;
    size_t size = token->text_size;
    size_t offset = 0u;
    int negative = size && bytes[0] == '-';
    if (negative) {
        ++offset;
    }
    uint64_t magnitude = 0u;
    for (; offset < size; ++offset) {
        unsigned char byte = (unsigned char)bytes[offset];
        if (byte < '0' || byte > '9') {
            return MAELYS_JSON_ERR_NOT_INTEGER;
        }
        uint64_t digit = byte - (unsigned char)'0';
        if (magnitude > (maximum - digit) / 10u) {
            return MAELYS_JSON_ERR_RANGE;
        }
        magnitude = magnitude * 10u + digit;
    }
    *out_magnitude = magnitude;
    *out_negative = negative;
    return MAELYS_JSON_OK;
}

maelys_json_result_t maelys_json_value_u64(
    const maelys_json_document_t *document, maelys_json_value_t value,
    uint64_t *out_number) {
    if (!out_number) {
        return MAELYS_JSON_ERR_ARGUMENT;
    }
    uint64_t magnitude;
    int negative;
    maelys_json_result_t result = read_magnitude(document, value, UINT64_MAX,
        &magnitude, &negative);
    if (result != MAELYS_JSON_OK) {
        return result;
    }
    if (negative && magnitude) {
        return MAELYS_JSON_ERR_RANGE;
    }
    *out_number = magnitude;
    return MAELYS_JSON_OK;
}

maelys_json_result_t maelys_json_value_i64(
    const maelys_json_document_t *document, maelys_json_value_t value,
    int64_t *out_number) {
    if (!out_number) {
        return MAELYS_JSON_ERR_ARGUMENT;
    }
    const uint64_t minimum_magnitude = UINT64_C(9223372036854775808);
    uint64_t magnitude;
    int negative;
    maelys_json_result_t result = read_magnitude(document, value,
        minimum_magnitude, &magnitude, &negative);
    if (result != MAELYS_JSON_OK) {
        return result;
    }
    if (!negative && magnitude > (uint64_t)INT64_MAX) {
        return MAELYS_JSON_ERR_RANGE;
    }
    if (negative && magnitude == minimum_magnitude) {
        *out_number = INT64_MIN;
    } else {
        *out_number = negative ? -(int64_t)magnitude : (int64_t)magnitude;
    }
    return MAELYS_JSON_OK;
}

maelys_json_result_t maelys_json_value_boolean(
    const maelys_json_document_t *document, maelys_json_value_t value,
    int *out_enabled) {
    if (!out_enabled) {
        return MAELYS_JSON_ERR_ARGUMENT;
    }
    const maelys_json_token_t *token;
    maelys_json_result_t result = token_of(document, value,
        MAELYS_JSON_TYPE_BOOLEAN, &token);
    if (result != MAELYS_JSON_OK) {
        return result;
    }
    *out_enabled = token->boolean_value != 0u;
    return MAELYS_JSON_OK;
}

int maelys_json_value_is_null(
    const maelys_json_document_t *document, maelys_json_value_t value) {
    return document && value < document->token_count &&
        document->tokens[value].type == MAELYS_JSON_TYPE_NULL;
}

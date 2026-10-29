/* SPDX-License-Identifier: MPL-2.0 */
#include "internal.h"

#include <stdio.h>
#include <string.h>

const char *maelys_json_version(void) {
    return MAELYS_JSON_VERSION_STRING;
}

size_t maelys_json_canonical_escape(unsigned char byte, char *out) {
    static const char hex[] = "0123456789abcdef";
    const char *escape = NULL;
    switch (byte) {
        case '"': escape = "\\\""; break;
        case '\\': escape = "\\\\"; break;
        case '\b': escape = "\\b"; break;
        case '\f': escape = "\\f"; break;
        case '\n': escape = "\\n"; break;
        case '\r': escape = "\\r"; break;
        case '\t': escape = "\\t"; break;
        default: break;
    }
    if (escape) {
        out[0] = escape[0];
        out[1] = escape[1];
        return 2u;
    }
    if (byte >= 0x20u) {
        out[0] = (char)byte;
        return 1u;
    }
    unsigned int code = byte;
    out[0] = '\\';
    out[1] = 'u';
    out[2] = '0';
    out[3] = '0';
    out[4] = hex[(code >> 4u) & 0xfu];
    out[5] = hex[code & 0xfu];
    return 6u;
}

int maelys_json_profile_valid(maelys_json_profile_t profile) {
    return profile == MAELYS_JSON_PROFILE_RFC8259 ||
        profile == MAELYS_JSON_PROFILE_CONTRACT_ASCII ||
        profile == MAELYS_JSON_PROFILE_RFC8259_LAST_WINS;
}

maelys_json_result_t maelys_json_limits_resolve(
    const maelys_json_limits_t *requested, maelys_json_limits_t *out_limits) {
    maelys_json_limits_t result = {
        .maximum_bytes = MAELYS_JSON_DEFAULT_MAXIMUM_BYTES,
        .maximum_depth = MAELYS_JSON_DEFAULT_MAXIMUM_DEPTH,
        .maximum_tokens = MAELYS_JSON_DEFAULT_MAXIMUM_TOKENS
    };
    if (requested) {
        if (requested->maximum_bytes) {
            result.maximum_bytes = requested->maximum_bytes;
        }
        if (requested->maximum_depth) {
            result.maximum_depth = requested->maximum_depth;
        }
        if (requested->maximum_tokens) {
            result.maximum_tokens = requested->maximum_tokens;
        }
    }
    /* maximum_bytes < SIZE_MAX / 2 keeps every size arithmetic in the
     * parser (arena of size + tokens + 1, file buffer of size + 1) exact. */
    if (result.maximum_depth > MAELYS_JSON_MAXIMUM_DEPTH ||
        result.maximum_bytes >= SIZE_MAX / 2u ||
        result.maximum_tokens > SIZE_MAX / 256u) {
        return MAELYS_JSON_ERR_ARGUMENT;
    }
    *out_limits = result;
    return MAELYS_JSON_OK;
}

void maelys_json_error_set(
    maelys_json_error_t *error, maelys_json_result_t code,
    const char *bytes, size_t size, size_t offset) {
    if (!error) {
        return;
    }
    if (offset > size) {
        offset = size;
    }
    *error = (maelys_json_error_t){
        .code = code, .offset = offset, .line = 1u, .column = 1u
    };
    for (size_t i = 0u; bytes && i < offset; ++i) {
        if (bytes[i] == '\n') {
            ++error->line;
            error->column = 1u;
        } else {
            ++error->column;
        }
    }
}

size_t maelys_json_error_format(
    const maelys_json_error_t *error, char *buffer, size_t capacity) {
    if (!error || (!buffer && capacity)) {
        return 0u;
    }
    int length = snprintf(buffer, capacity, "line %zu, column %zu (offset %zu): %s",
        error->line, error->column, error->offset,
        maelys_json_result_string(error->code));
    return length < 0 ? 0u : (size_t)length;
}

static int is_digit(char byte) {
    return byte >= '0' && byte <= '9';
}

int maelys_json_number_syntax(const char *bytes, size_t size) {
    size_t offset = 0u;
    if (offset < size && bytes[offset] == '-') {
        ++offset;
    }
    if (offset >= size) {
        return 0;
    }
    if (bytes[offset] == '0') {
        ++offset;
    } else {
        if (bytes[offset] < '1' || bytes[offset] > '9') {
            return 0;
        }
        while (offset < size && is_digit(bytes[offset])) {
            ++offset;
        }
    }
    if (offset < size && bytes[offset] == '.') {
        size_t fraction = ++offset;
        while (offset < size && is_digit(bytes[offset])) {
            ++offset;
        }
        if (offset == fraction) {
            return 0;
        }
    }
    if (offset < size && (bytes[offset] == 'e' || bytes[offset] == 'E')) {
        ++offset;
        if (offset < size && (bytes[offset] == '+' || bytes[offset] == '-')) {
            ++offset;
        }
        size_t exponent = offset;
        while (offset < size && is_digit(bytes[offset])) {
            ++offset;
        }
        if (offset == exponent) {
            return 0;
        }
    }
    return offset == size;
}

const char *maelys_json_result_string(maelys_json_result_t result) {
    switch (result) {
        case MAELYS_JSON_OK: return "ok";
        case MAELYS_JSON_ERR_ARGUMENT: return "invalid argument";
        case MAELYS_JSON_ERR_MEMORY: return "out of memory";
        case MAELYS_JSON_ERR_LIMIT: return "configured limit exceeded";
        case MAELYS_JSON_ERR_SYNTAX: return "invalid JSON syntax";
        case MAELYS_JSON_ERR_UTF8: return "invalid UTF-8";
        case MAELYS_JSON_ERR_DUPLICATE_KEY: return "duplicate object key";
        case MAELYS_JSON_ERR_TYPE: return "unexpected JSON type";
        case MAELYS_JSON_ERR_RANGE: return "numeric value out of range";
        case MAELYS_JSON_ERR_STATE: return "invalid writer state";
        case MAELYS_JSON_ERR_NOT_FOUND: return "object member not found";
        case MAELYS_JSON_ERR_NOT_INTEGER: return "number is not an integer";
        case MAELYS_JSON_ERR_IO: return "input/output error";
    }
    return "unknown JSON error";
}

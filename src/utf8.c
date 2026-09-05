/* SPDX-License-Identifier: MPL-2.0 */
#include "internal.h"

#include <string.h>

static int continuation(unsigned char byte) {
    return (byte & 0xc0u) == 0x80u;
}

/* Returns the size of the longest valid UTF-8 prefix of `bytes`. */
static size_t utf8_valid_prefix(const unsigned char *bytes, size_t size) {
    size_t i = 0u;
    while (i < size) {
        unsigned char a = bytes[i];
        if (a <= 0x7fu) {
            ++i;
            continue;
        }
        if (a >= 0xc2u && a <= 0xdfu) {
            if (i + 1u >= size || !continuation(bytes[i + 1u])) {
                return i;
            }
            i += 2u;
            continue;
        }
        if (a >= 0xe0u && a <= 0xefu) {
            if (i + 2u >= size || !continuation(bytes[i + 1u]) ||
                !continuation(bytes[i + 2u])) {
                return i;
            }
            if ((a == 0xe0u && bytes[i + 1u] < 0xa0u) ||
                (a == 0xedu && bytes[i + 1u] >= 0xa0u)) {
                return i;
            }
            i += 3u;
            continue;
        }
        if (a >= 0xf0u && a <= 0xf4u) {
            if (i + 3u >= size || !continuation(bytes[i + 1u]) ||
                !continuation(bytes[i + 2u]) || !continuation(bytes[i + 3u])) {
                return i;
            }
            if ((a == 0xf0u && bytes[i + 1u] < 0x90u) ||
                (a == 0xf4u && bytes[i + 1u] >= 0x90u)) {
                return i;
            }
            i += 4u;
            continue;
        }
        return i;
    }
    return size;
}

int maelys_json_utf8_validate(const unsigned char *bytes, size_t size) {
    return utf8_valid_prefix(bytes, size) == size;
}

size_t maelys_json_utf8_decode(
    const unsigned char *bytes, size_t size, uint32_t *out_point) {
    /* `size` only guards the assertions: the input is validated UTF-8. */
    (void)size;
    MAELYS_JSON_ASSERT(size >= 1u);
    unsigned char a = bytes[0];
    if (a <= 0x7fu) {
        *out_point = a;
        return 1u;
    }
    if (a <= 0xdfu) {
        MAELYS_JSON_ASSERT(size >= 2u);
        *out_point = ((uint32_t)(a & 0x1fu) << 6u) | (uint32_t)(bytes[1] & 0x3fu);
        return 2u;
    }
    if (a <= 0xefu) {
        MAELYS_JSON_ASSERT(size >= 3u);
        *out_point = ((uint32_t)(a & 0x0fu) << 12u) |
            ((uint32_t)(bytes[1] & 0x3fu) << 6u) | (uint32_t)(bytes[2] & 0x3fu);
        return 3u;
    }
    MAELYS_JSON_ASSERT(size >= 4u);
    *out_point = ((uint32_t)(a & 0x07u) << 18u) |
        ((uint32_t)(bytes[1] & 0x3fu) << 12u) |
        ((uint32_t)(bytes[2] & 0x3fu) << 6u) | (uint32_t)(bytes[3] & 0x3fu);
    return 4u;
}

static int hex_value(unsigned char byte) {
    if (byte >= '0' && byte <= '9') {
        return byte - '0';
    }
    if (byte >= 'a' && byte <= 'f') {
        return byte - 'a' + 10;
    }
    if (byte >= 'A' && byte <= 'F') {
        return byte - 'A' + 10;
    }
    return -1;
}

static int read_u16(const char *bytes, uint32_t *out) {
    uint32_t value = 0u;
    for (size_t i = 0u; i < 4u; ++i) {
        int digit = hex_value((unsigned char)bytes[i]);
        if (digit < 0) {
            return 0;
        }
        value = value * 16u + (uint32_t)digit;
    }
    *out = value;
    return 1;
}

static size_t encode_utf8(uint32_t point, char out[4]) {
    if (point <= 0x7fu) {
        out[0] = (char)point;
        return 1u;
    }
    if (point <= 0x7ffu) {
        out[0] = (char)(0xc0u | (point >> 6u));
        out[1] = (char)(0x80u | (point & 0x3fu));
        return 2u;
    }
    if (point <= 0xffffu) {
        out[0] = (char)(0xe0u | (point >> 12u));
        out[1] = (char)(0x80u | ((point >> 6u) & 0x3fu));
        out[2] = (char)(0x80u | (point & 0x3fu));
        return 3u;
    }
    out[0] = (char)(0xf0u | (point >> 18u));
    out[1] = (char)(0x80u | ((point >> 12u) & 0x3fu));
    out[2] = (char)(0x80u | ((point >> 6u) & 0x3fu));
    out[3] = (char)(0x80u | (point & 0x3fu));
    return 4u;
}

/* Reads a \uXXXX escape (and its low surrogate when needed) starting after
 * the 'u'. Returns the decoded code point or 0 for a syntax error; U+0000 is
 * rejected by design so 0 never denotes a valid result. */
static uint32_t read_escape_point(const char *bytes, size_t size,
    size_t *input) {
    if (*input + 4u > size) {
        return 0u;
    }
    uint32_t point;
    if (!read_u16(bytes + *input, &point)) {
        return 0u;
    }
    *input += 4u;
    if (point >= 0xd800u && point <= 0xdbffu) {
        if (*input + 6u > size || bytes[*input] != '\\' ||
            bytes[*input + 1u] != 'u') {
            return 0u;
        }
        uint32_t low;
        if (!read_u16(bytes + *input + 2u, &low) || low < 0xdc00u ||
            low > 0xdfffu) {
            return 0u;
        }
        *input += 6u;
        return 0x10000u + ((point - 0xd800u) << 10u) + (low - 0xdc00u);
    }
    if (point >= 0xdc00u && point <= 0xdfffu) {
        return 0u;
    }
    return point;
}

static int simple_escape(unsigned char escape, char *out) {
    switch (escape) {
        case '"': *out = '"'; return 1;
        case '\\': *out = '\\'; return 1;
        case '/': *out = '/'; return 1;
        case 'b': *out = '\b'; return 1;
        case 'f': *out = '\f'; return 1;
        case 'n': *out = '\n'; return 1;
        case 'r': *out = '\r'; return 1;
        case 't': *out = '\t'; return 1;
        default: return 0;
    }
}

maelys_json_result_t maelys_json_decode_string(
    const char *bytes, size_t size, maelys_json_profile_t profile,
    char *out, size_t *out_size, size_t *out_bad_offset) {
    MAELYS_JSON_ASSERT(out && out_size && (bytes || !size));
    size_t input = 0u;
    size_t output = 0u;
    while (input < size) {
        unsigned char byte = (unsigned char)bytes[input];
        if (profile == MAELYS_JSON_PROFILE_CONTRACT_ASCII) {
            if (byte < 0x20u || byte > 0x7eu || byte == '\\') {
                *out_bad_offset = input;
                return MAELYS_JSON_ERR_SYNTAX;
            }
            out[output++] = (char)byte;
            ++input;
            continue;
        }
        if (byte != '\\') {
            size_t start = input;
            while (input < size && bytes[input] != '\\') {
                ++input;
            }
            size_t valid = utf8_valid_prefix(
                (const unsigned char *)bytes + start, input - start);
            if (valid != input - start) {
                *out_bad_offset = start + valid;
                return MAELYS_JSON_ERR_UTF8;
            }
            memcpy(out + output, bytes + start, input - start);
            output += input - start;
            continue;
        }
        size_t escape_offset = input++;
        if (input >= size) {
            *out_bad_offset = escape_offset;
            return MAELYS_JSON_ERR_SYNTAX;
        }
        unsigned char escape = (unsigned char)bytes[input++];
        if (escape == 'u') {
            uint32_t point = read_escape_point(bytes, size, &input);
            if (!point) {
                *out_bad_offset = escape_offset;
                return MAELYS_JSON_ERR_SYNTAX;
            }
            char encoded[4];
            size_t encoded_size = encode_utf8(point, encoded);
            memcpy(out + output, encoded, encoded_size);
            output += encoded_size;
        } else if (!simple_escape(escape, out + output)) {
            *out_bad_offset = escape_offset;
            return MAELYS_JSON_ERR_SYNTAX;
        } else {
            ++output;
        }
    }
    out[output] = '\0';
    *out_size = output;
    return MAELYS_JSON_OK;
}

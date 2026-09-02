#include "writer_internal.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INDENT_WIDTH 2u

typedef struct serializer {
    const maelys_json_writer_t *writer;
    char *bytes;
    size_t size;
    size_t capacity;
    size_t maximum;
    int indent;
    int ascii;
} serializer_t;

static maelys_json_result_t reserve(serializer_t *s, size_t extra) {
    if (extra > s->maximum - s->size) {
        return MAELYS_JSON_ERR_LIMIT;
    }
    size_t needed = s->size + extra;
    if (needed <= s->capacity) {
        return MAELYS_JSON_OK;
    }
    size_t capacity = s->capacity ? s->capacity : 128u;
    while (capacity < needed) {
        capacity = capacity > s->maximum / 2u ? s->maximum : capacity * 2u;
    }
    char *bytes = realloc(s->bytes, capacity + 1u);
    if (!bytes) {
        return MAELYS_JSON_ERR_MEMORY;
    }
    s->bytes = bytes;
    s->capacity = capacity;
    return MAELYS_JSON_OK;
}

static maelys_json_result_t append(
    serializer_t *s, const char *bytes, size_t size) {
    maelys_json_result_t result = reserve(s, size);
    if (result != MAELYS_JSON_OK) {
        return result;
    }
    memcpy(s->bytes + s->size, bytes, size);
    s->size += size;
    return MAELYS_JSON_OK;
}

static maelys_json_result_t append_byte(serializer_t *s, char byte) {
    return append(s, &byte, 1u);
}

static maelys_json_result_t append_literal(serializer_t *s, const char *text) {
    return append(s, text, strlen(text));
}

/* Newline followed by the indentation of `level`; nothing in compact mode. */
static maelys_json_result_t append_break(serializer_t *s, size_t level) {
    if (!s->indent) {
        return MAELYS_JSON_OK;
    }
    maelys_json_result_t result = append_byte(s, '\n');
    for (size_t i = 0u; result == MAELYS_JSON_OK && i < level * INDENT_WIDTH; ++i) {
        result = append_byte(s, ' ');
    }
    return result;
}

static maelys_json_result_t append_u16_escape(serializer_t *s, uint32_t unit) {
    static const char hex[] = "0123456789abcdef";
    char encoded[6] = {'\\', 'u', hex[(unit >> 12u) & 0xfu],
        hex[(unit >> 8u) & 0xfu], hex[(unit >> 4u) & 0xfu], hex[unit & 0xfu]};
    return append(s, encoded, sizeof(encoded));
}

static const char *short_escape(unsigned char byte) {
    switch (byte) {
        case '"': return "\\\"";
        case '\\': return "\\\\";
        case '\b': return "\\b";
        case '\f': return "\\f";
        case '\n': return "\\n";
        case '\r': return "\\r";
        case '\t': return "\\t";
        default: return NULL;
    }
}

static maelys_json_result_t append_string(
    serializer_t *s, const char *bytes, size_t size) {
    maelys_json_result_t result = append_byte(s, '"');
    size_t i = 0u;
    while (result == MAELYS_JSON_OK && i < size) {
        unsigned char byte = (unsigned char)bytes[i];
        const char *escape = short_escape(byte);
        if (escape) {
            result = append(s, escape, 2u);
            ++i;
        } else if (byte < 0x20u) {
            result = append_u16_escape(s, byte);
            ++i;
        } else if (byte >= 0x80u && s->ascii) {
            uint32_t point;
            i += maelys_json_utf8_decode((const unsigned char *)bytes + i,
                size - i, &point);
            if (point >= 0x10000u) {
                point -= 0x10000u;
                result = append_u16_escape(s, 0xd800u + (point >> 10u));
                if (result == MAELYS_JSON_OK) {
                    result = append_u16_escape(s, 0xdc00u + (point & 0x3ffu));
                }
            } else {
                result = append_u16_escape(s, point);
            }
        } else {
            result = append_byte(s, (char)byte);
            ++i;
        }
    }
    if (result == MAELYS_JSON_OK) {
        result = append_byte(s, '"');
    }
    return result;
}

static maelys_json_result_t append_number(
    serializer_t *s, const maelys_json_node_t *node) {
    char number[32];
    int length = node->number_signed ?
        snprintf(number, sizeof(number), "%" PRId64, node->signed_value) :
        snprintf(number, sizeof(number), "%" PRIu64, node->unsigned_value);
    MAELYS_JSON_ASSERT(length > 0 && (size_t)length < sizeof(number));
    return append(s, number, (size_t)length);
}

/* Streams the UTF-16 code units of a validated UTF-8 key. */
typedef struct utf16_cursor {
    const unsigned char *bytes;
    size_t size;
    size_t offset;
    uint32_t pending_low_surrogate;
} utf16_cursor_t;

static int utf16_next(utf16_cursor_t *cursor, uint32_t *out_unit) {
    if (cursor->pending_low_surrogate) {
        *out_unit = cursor->pending_low_surrogate;
        cursor->pending_low_surrogate = 0u;
        return 1;
    }
    if (cursor->offset >= cursor->size) {
        return 0;
    }
    uint32_t point;
    cursor->offset += maelys_json_utf8_decode(cursor->bytes + cursor->offset,
        cursor->size - cursor->offset, &point);
    if (point >= 0x10000u) {
        point -= 0x10000u;
        *out_unit = 0xd800u + (point >> 10u);
        cursor->pending_low_surrogate = 0xdc00u + (point & 0x3ffu);
    } else {
        *out_unit = point;
    }
    return 1;
}

/* Key order of RFC 8785 section 3.2.3: unescaped names compared as arrays
 * of UTF-16 code units treated as unsigned integers; a prefix sorts first.
 * This equals code point order except that supplementary characters
 * (U+10000 and above) sort before U+E000..U+FFFF. */
static int key_compare(
    const maelys_json_writer_t *writer, size_t left, size_t right) {
    const maelys_json_node_t *a = &writer->nodes[left];
    const maelys_json_node_t *b = &writer->nodes[right];
    utf16_cursor_t ca = {(const unsigned char *)a->key, a->key_size, 0u, 0u};
    utf16_cursor_t cb = {(const unsigned char *)b->key, b->key_size, 0u, 0u};
    for (;;) {
        uint32_t ua;
        uint32_t ub;
        int has_a = utf16_next(&ca, &ua);
        int has_b = utf16_next(&cb, &ub);
        if (!has_a || !has_b) {
            return has_a - has_b;
        }
        if (ua != ub) {
            return ua < ub ? -1 : 1;
        }
    }
}

static void merge_keys(
    const maelys_json_writer_t *writer, size_t *values, size_t *scratch,
    size_t begin, size_t middle, size_t end) {
    size_t left = begin;
    size_t right = middle;
    size_t output = begin;
    while (left < middle || right < end) {
        if (right >= end || (left < middle &&
            key_compare(writer, values[left], values[right]) <= 0)) {
            scratch[output++] = values[left++];
        } else {
            scratch[output++] = values[right++];
        }
    }
    memcpy(values + begin, scratch + begin, (end - begin) * sizeof(*values));
}

static void sort_keys(
    const maelys_json_writer_t *writer, size_t *values, size_t *scratch,
    size_t begin, size_t end) {
    if (end - begin < 2u) {
        return;
    }
    size_t middle = begin + (end - begin) / 2u;
    sort_keys(writer, values, scratch, begin, middle);
    sort_keys(writer, values, scratch, middle, end);
    merge_keys(writer, values, scratch, begin, middle, end);
}

static maelys_json_result_t serialize_node(
    serializer_t *s, size_t index, size_t level);

static maelys_json_result_t serialize_object(
    serializer_t *s, const maelys_json_node_t *node, size_t level) {
    size_t count = node->links.child_count;
    if (!count) {
        return append_literal(s, "{}");
    }
    size_t *values = malloc(count * sizeof(*values));
    size_t *scratch = malloc(count * sizeof(*scratch));
    maelys_json_result_t result = values && scratch ? MAELYS_JSON_OK :
        MAELYS_JSON_ERR_MEMORY;
    if (result == MAELYS_JSON_OK) {
        size_t position = 0u;
        for (size_t child = node->links.first_child;
             child != MAELYS_JSON_VALUE_NONE;
             child = s->writer->nodes[child].links.next_sibling) {
            values[position++] = child;
        }
        MAELYS_JSON_ASSERT(position == count);
        sort_keys(s->writer, values, scratch, 0u, count);
        result = append_byte(s, '{');
    }
    for (size_t i = 0u; result == MAELYS_JSON_OK && i < count; ++i) {
        const maelys_json_node_t *child = &s->writer->nodes[values[i]];
        if (i) {
            result = append_byte(s, ',');
        }
        if (result == MAELYS_JSON_OK) {
            result = append_break(s, level + 1u);
        }
        if (result == MAELYS_JSON_OK) {
            result = append_string(s, child->key, child->key_size);
        }
        if (result == MAELYS_JSON_OK) {
            result = append_literal(s, s->indent ? ": " : ":");
        }
        if (result == MAELYS_JSON_OK) {
            result = serialize_node(s, values[i], level + 1u);
        }
    }
    if (result == MAELYS_JSON_OK) {
        result = append_break(s, level);
    }
    if (result == MAELYS_JSON_OK) {
        result = append_byte(s, '}');
    }
    free(scratch);
    free(values);
    return result;
}

static maelys_json_result_t serialize_array(
    serializer_t *s, const maelys_json_node_t *node, size_t level) {
    if (!node->links.child_count) {
        return append_literal(s, "[]");
    }
    maelys_json_result_t result = append_byte(s, '[');
    size_t position = 0u;
    for (size_t child = node->links.first_child;
         result == MAELYS_JSON_OK && child != MAELYS_JSON_VALUE_NONE;
         child = s->writer->nodes[child].links.next_sibling, ++position) {
        if (position) {
            result = append_byte(s, ',');
        }
        if (result == MAELYS_JSON_OK) {
            result = append_break(s, level + 1u);
        }
        if (result == MAELYS_JSON_OK) {
            result = serialize_node(s, child, level + 1u);
        }
    }
    if (result == MAELYS_JSON_OK) {
        result = append_break(s, level);
    }
    if (result == MAELYS_JSON_OK) {
        result = append_byte(s, ']');
    }
    return result;
}

static maelys_json_result_t serialize_node(
    serializer_t *s, size_t index, size_t level) {
    const maelys_json_node_t *node = &s->writer->nodes[index];
    switch (node->type) {
        case MAELYS_JSON_TYPE_OBJECT:
            return serialize_object(s, node, level);
        case MAELYS_JSON_TYPE_ARRAY:
            return serialize_array(s, node, level);
        case MAELYS_JSON_TYPE_STRING:
            return append_string(s, node->string, node->string_size);
        case MAELYS_JSON_TYPE_NUMBER:
            return append_number(s, node);
        case MAELYS_JSON_TYPE_BOOLEAN:
            return append_literal(s, node->boolean_value ? "true" : "false");
        case MAELYS_JSON_TYPE_NULL:
            return append_literal(s, "null");
        case MAELYS_JSON_TYPE_NONE:
            break;
    }
    /* Only MAELYS_JSON_TYPE_NONE reaches this point, and no node has it. */
    MAELYS_JSON_ASSERT(node->type != MAELYS_JSON_TYPE_NONE);
    return MAELYS_JSON_ERR_STATE;
}

maelys_json_result_t maelys_json_serialize(
    const maelys_json_writer_t *writer, char **out_bytes, size_t *out_size) {
    serializer_t s = {
        .writer = writer, .maximum = writer->limits.maximum_bytes,
        .indent = (writer->flags & MAELYS_JSON_WRITER_INDENT) != 0u,
        .ascii = (writer->flags & MAELYS_JSON_WRITER_ASCII) != 0u
    };
    maelys_json_result_t result = serialize_node(&s, 0u, 0u);
    if (result == MAELYS_JSON_OK &&
        (writer->flags & MAELYS_JSON_WRITER_FINAL_NEWLINE)) {
        result = append_byte(&s, '\n');
    }
    if (result != MAELYS_JSON_OK) {
        free(s.bytes);
        return result;
    }
    MAELYS_JSON_ASSERT(s.bytes && s.size <= s.capacity);
    s.bytes[s.size] = '\0';
    *out_bytes = s.bytes;
    *out_size = s.size;
    return MAELYS_JSON_OK;
}

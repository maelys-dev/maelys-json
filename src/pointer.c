/* SPDX-License-Identifier: MPL-2.0 */
#include "internal.h"

#include <stdlib.h>
#include <string.h>

/*
 * JSON Pointer of the value under construction at a byte offset, obtained
 * by a structural rescan of the input. The scan does not validate: it only
 * tracks containers, member keys and element indices, and stops at the
 * offset. It runs on error paths only.
 */

typedef struct frame {
    int is_object;
    size_t index;
    size_t key_start;
    size_t key_end;
    int has_key;
    int in_value; /* object: the key's colon was seen */
} frame_t;

typedef struct emitter {
    char *buffer;
    size_t capacity;
    size_t length;
} emitter_t;

static void emit_byte(emitter_t *emitter, char byte) {
    if (emitter->length + 1u < emitter->capacity) {
        emitter->buffer[emitter->length] = byte;
    }
    ++emitter->length;
}

static void emit_finish(emitter_t *emitter) {
    if (emitter->capacity) {
        size_t end = emitter->length < emitter->capacity ? emitter->length :
            emitter->capacity - 1u;
        emitter->buffer[end] = '\0';
    }
}

/* Appends "/" then the key with RFC 6901 escaping. */
static void emit_token(emitter_t *emitter, const char *bytes, size_t size) {
    emit_byte(emitter, '/');
    for (size_t i = 0u; i < size; ++i) {
        if (bytes[i] == '~') {
            emit_byte(emitter, '~');
            emit_byte(emitter, '0');
        } else if (bytes[i] == '/') {
            emit_byte(emitter, '~');
            emit_byte(emitter, '1');
        } else {
            emit_byte(emitter, bytes[i]);
        }
    }
}

static void emit_index(emitter_t *emitter, size_t index) {
    char digits[32];
    size_t count = 0u;
    do {
        digits[count++] = (char)('0' + index % 10u);
        index /= 10u;
    } while (index);
    emit_byte(emitter, '/');
    while (count) {
        emit_byte(emitter, digits[--count]);
    }
}

/* Decodes the key when possible, otherwise emits its raw bytes. */
static void emit_key(emitter_t *emitter, const char *bytes, const frame_t *frame) {
    size_t size = frame->key_end - frame->key_start;
    const char *raw = bytes + frame->key_start;
    char *decoded = malloc(size + 1u);
    size_t decoded_size;
    size_t bad;
    if (decoded && maelys_json_decode_string(raw, size,
            MAELYS_JSON_PROFILE_RFC8259, decoded, &decoded_size, &bad) ==
            MAELYS_JSON_OK) {
        emit_token(emitter, decoded, decoded_size);
    } else {
        emit_token(emitter, raw, size);
    }
    free(decoded);
}

static int is_space(char byte) {
    return byte == ' ' || byte == '\t' || byte == '\r' || byte == '\n';
}

static int is_delimiter(char byte) {
    return byte == ',' || byte == '}' || byte == ']' || byte == ':' ||
        is_space(byte);
}

/* Returns the offset just past the closing quote of the string literal
 * opening at `pos`, or `size` when unterminated. */
static size_t skip_string(const char *bytes, size_t size, size_t pos) {
    ++pos;
    while (pos < size && bytes[pos] != '"') {
        pos += bytes[pos] == '\\' ? 2u : 1u;
    }
    return pos < size ? pos + 1u : size;
}

static void scan(const char *bytes, size_t size, size_t limit, frame_t *frames,
    size_t *out_depth) {
    size_t depth = 0u;
    size_t pos = 0u;
    while (pos < limit) {
        char byte = bytes[pos];
        frame_t *top = depth ? &frames[depth - 1u] : NULL;
        if (is_space(byte)) {
            ++pos;
        } else if (byte == '{' || byte == '[') {
            if (depth < MAELYS_JSON_MAXIMUM_DEPTH) {
                frames[depth++] = (frame_t){.is_object = byte == '{'};
            }
            ++pos;
        } else if (byte == '}' || byte == ']') {
            if (depth) {
                --depth;
            }
            ++pos;
        } else if (byte == ',') {
            if (top && top->is_object) {
                top->has_key = 0;
                top->in_value = 0;
            } else if (top) {
                ++top->index;
            }
            ++pos;
        } else if (byte == ':') {
            if (top && top->is_object && top->has_key) {
                top->in_value = 1;
            }
            ++pos;
        } else if (byte == '"') {
            size_t end = skip_string(bytes, size, pos);
            if (top && top->is_object && !top->in_value) {
                top->key_start = pos + 1u;
                top->key_end = end > pos + 1u && bytes[end - 1u] == '"' &&
                    end - 1u > pos ? end - 1u : end;
                top->has_key = 1;
            }
            pos = end;
        } else {
            while (pos < size && !is_delimiter(bytes[pos])) {
                ++pos;
            }
        }
    }
    *out_depth = depth;
}

size_t maelys_json_error_pointer(
    const void *bytes, size_t size, const maelys_json_error_t *error,
    char *buffer, size_t capacity) {
    emitter_t emitter = {.buffer = buffer, .capacity = buffer ? capacity : 0u};
    if (!error || !bytes || !size) {
        /* No error, or nothing to scan: the pointer is the root. */
        emit_finish(&emitter);
        return 0u;
    }
    size_t limit = error->offset < size ? error->offset : size;
    frame_t *frames = calloc(MAELYS_JSON_MAXIMUM_DEPTH, sizeof(*frames));
    if (!frames) {
        emit_finish(&emitter);
        return 0u;
    }
    size_t depth;
    scan(bytes, size, limit, frames, &depth);
    for (size_t i = 0u; i < depth; ++i) {
        const frame_t *frame = &frames[i];
        if (frame->is_object) {
            if (!frame->has_key || !frame->in_value) {
                break; /* inside a key or before its colon */
            }
            emit_key(&emitter, bytes, frame);
        } else {
            emit_index(&emitter, frame->index);
        }
    }
    free(frames);
    emit_finish(&emitter);
    return emitter.length;
}

/* SPDX-License-Identifier: MPL-2.0 */
#ifndef MAELYS_JSON_INTERNAL_H
#define MAELYS_JSON_INTERNAL_H

#include "maelys/json.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__GNUC__)
#define MAELYS_JSON_INTERNAL __attribute__((visibility("hidden")))
#else
#define MAELYS_JSON_INTERNAL
#endif

/* Internal invariants. Violations are library bugs, never caller errors. */
#define MAELYS_JSON_ASSERT(condition) assert(condition)

/* Parent and sibling links shared by parser tokens and writer nodes. */
typedef struct maelys_json_links {
    size_t parent;
    size_t first_child;
    size_t last_child;
    size_t next_sibling;
    size_t child_count;
} maelys_json_links_t;

static inline void maelys_json_links_init(
    maelys_json_links_t *links, size_t parent) {
    links->parent = parent;
    links->first_child = MAELYS_JSON_VALUE_NONE;
    links->last_child = MAELYS_JSON_VALUE_NONE;
    links->next_sibling = MAELYS_JSON_VALUE_NONE;
    links->child_count = 0u;
}

/* Appends `index` as the last child of `container`. `last` is the links of
 * the current last child, or NULL when the container is empty. */
static inline void maelys_json_links_append(
    maelys_json_links_t *container, maelys_json_links_t *last, size_t index) {
    if (last) {
        last->next_sibling = index;
    } else {
        container->first_child = index;
    }
    container->last_child = index;
    ++container->child_count;
}

typedef struct maelys_json_token {
    maelys_json_links_t links;
    maelys_json_type_t type;
    size_t start;
    size_t end;
    /* Decoded string, or number lexeme. NUL-terminated, lives in the arena. */
    const char *text;
    size_t text_size;
    /* Containers: first index into document->children. */
    size_t children_offset;
    unsigned char object_key;
    unsigned char boolean_value;
} maelys_json_token_t;

struct maelys_json_document {
    maelys_json_profile_t profile;
    char *bytes;
    size_t size;
    maelys_json_token_t *tokens;
    size_t token_count;
    size_t token_capacity;
    char *arena;
    size_t arena_used;
    size_t arena_capacity;
    /* Children of every container, contiguous per container. */
    size_t *children;
};

MAELYS_JSON_INTERNAL int maelys_json_profile_valid(
    maelys_json_profile_t profile);
MAELYS_JSON_INTERNAL maelys_json_result_t maelys_json_limits_resolve(
    const maelys_json_limits_t *requested, maelys_json_limits_t *out_limits);
MAELYS_JSON_INTERNAL void maelys_json_error_set(
    maelys_json_error_t *error, maelys_json_result_t code,
    const char *bytes, size_t size, size_t offset);
MAELYS_JSON_INTERNAL int maelys_json_number_syntax(
    const char *bytes, size_t size);

MAELYS_JSON_INTERNAL int maelys_json_utf8_validate(
    const unsigned char *bytes, size_t size);
/* Decodes one code point from validated UTF-8. Returns its byte length. */
MAELYS_JSON_INTERNAL size_t maelys_json_utf8_decode(
    const unsigned char *bytes, size_t size, uint32_t *out_point);
/* Decodes the content of a JSON string literal into `out`, whose capacity
 * must be at least size + 1. Writes a trailing NUL. */
MAELYS_JSON_INTERNAL maelys_json_result_t maelys_json_decode_string(
    const char *bytes, size_t size, maelys_json_profile_t profile,
    char *out, size_t *out_size, size_t *out_bad_offset);

#endif

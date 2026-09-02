#ifndef MAELYS_JSON_KEYSET_H
#define MAELYS_JSON_KEYSET_H

#include "internal.h"

/*
 * Set of (parent, key bytes) pairs backed by an open-addressing table of
 * node indices. The set never copies keys: it asks the owner for the key of
 * an index through `key_of`, so it works for both parser tokens and writer
 * nodes, including when the owner reallocates its node array.
 */

typedef struct maelys_json_keyset_key {
    size_t parent;
    const char *bytes;
    size_t size;
} maelys_json_keyset_key_t;

typedef maelys_json_keyset_key_t (*maelys_json_keyset_key_fn)(
    const void *context, size_t index);

typedef struct maelys_json_keyset {
    size_t *slots;
    size_t capacity;
    size_t count;
    maelys_json_keyset_key_fn key_of;
    const void *context;
} maelys_json_keyset_t;

/* `expected` sizes the initial table; the set grows beyond it if needed. */
MAELYS_JSON_INTERNAL maelys_json_result_t maelys_json_keyset_init(
    maelys_json_keyset_t *set, size_t expected,
    maelys_json_keyset_key_fn key_of, const void *context);
MAELYS_JSON_INTERNAL void maelys_json_keyset_release(
    maelys_json_keyset_t *set);
MAELYS_JSON_INTERNAL int maelys_json_keyset_contains(
    const maelys_json_keyset_t *set, maelys_json_keyset_key_t key);
/* Errors: DUPLICATE_KEY when (parent, key) is present, MEMORY. */
MAELYS_JSON_INTERNAL maelys_json_result_t maelys_json_keyset_insert(
    maelys_json_keyset_t *set, size_t index);

#endif

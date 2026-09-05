/* SPDX-License-Identifier: MPL-2.0 */
#ifndef MAELYS_JSON_WRITER_INTERNAL_H
#define MAELYS_JSON_WRITER_INTERNAL_H

#include "internal.h"
#include "keyset.h"

typedef struct maelys_json_node {
    maelys_json_links_t links;
    maelys_json_type_t type;
    char *key;
    size_t key_size;
    char *string;
    size_t string_size;
    uint64_t unsigned_value;
    int64_t signed_value;
    unsigned char number_signed;
    unsigned char boolean_value;
} maelys_json_node_t;

struct maelys_json_writer {
    maelys_json_profile_t profile;
    maelys_json_limits_t limits;
    unsigned int flags;
    maelys_json_node_t *nodes;
    size_t node_count;
    size_t node_capacity;
    maelys_json_keyset_t keys;
    size_t *stack;
    size_t depth;
    char *pending_key;
    size_t pending_key_size;
    int finished;
    maelys_json_result_t failure;
};

/* Serializes the finished tree according to writer->flags. */
MAELYS_JSON_INTERNAL maelys_json_result_t maelys_json_serialize(
    const maelys_json_writer_t *writer, char **out_bytes, size_t *out_size);

#endif

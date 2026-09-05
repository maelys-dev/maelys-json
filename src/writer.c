/* SPDX-License-Identifier: MPL-2.0 */
#include "writer_internal.h"

#include <stdlib.h>
#include <string.h>

#define WRITER_INITIAL_NODES 16u
#define WRITER_INITIAL_KEYS 16u

static maelys_json_keyset_key_t node_key(const void *context, size_t index) {
    const maelys_json_writer_t *writer = context;
    const maelys_json_node_t *node = &writer->nodes[index];
    return (maelys_json_keyset_key_t){
        .parent = node->links.parent, .bytes = node->key, .size = node->key_size
    };
}

/* Records the first sticky error and returns it. */
static maelys_json_result_t writer_fail(
    maelys_json_writer_t *writer, maelys_json_result_t result) {
    MAELYS_JSON_ASSERT(result != MAELYS_JSON_OK &&
        result != MAELYS_JSON_ERR_ARGUMENT);
    if (writer->failure == MAELYS_JSON_OK) {
        writer->failure = result;
    }
    return result;
}

static maelys_json_result_t writer_ready(const maelys_json_writer_t *writer) {
    if (!writer) {
        return MAELYS_JSON_ERR_ARGUMENT;
    }
    if (writer->finished || writer->failure != MAELYS_JSON_OK) {
        return MAELYS_JSON_ERR_STATE;
    }
    return MAELYS_JSON_OK;
}

static int valid_text(
    maelys_json_profile_t profile, const char *bytes, size_t size) {
    if (!bytes && size) {
        return 0;
    }
    if (profile == MAELYS_JSON_PROFILE_RFC8259) {
        for (size_t i = 0u; i < size; ++i) {
            if (bytes[i] == '\0') {
                return 0;
            }
        }
        return maelys_json_utf8_validate((const unsigned char *)bytes, size);
    }
    for (size_t i = 0u; i < size; ++i) {
        unsigned char byte = (unsigned char)bytes[i];
        if (byte < 0x20u || byte > 0x7eu || byte == '"' || byte == '\\') {
            return 0;
        }
    }
    return 1;
}

static char *copy_bytes(const char *bytes, size_t size) {
    char *copy = malloc(size + 1u);
    if (!copy) {
        return NULL;
    }
    if (size) {
        memcpy(copy, bytes, size);
    }
    copy[size] = '\0';
    return copy;
}

static maelys_json_result_t grow_nodes(maelys_json_writer_t *writer) {
    if (writer->node_count >= writer->limits.maximum_tokens) {
        return MAELYS_JSON_ERR_LIMIT;
    }
    if (writer->node_count < writer->node_capacity) {
        return MAELYS_JSON_OK;
    }
    size_t capacity = writer->node_capacity ? writer->node_capacity * 2u :
        WRITER_INITIAL_NODES;
    if (capacity > writer->limits.maximum_tokens) {
        capacity = writer->limits.maximum_tokens;
    }
    maelys_json_node_t *nodes = realloc(writer->nodes,
        capacity * sizeof(*nodes));
    if (!nodes) {
        return MAELYS_JSON_ERR_MEMORY;
    }
    writer->nodes = nodes;
    writer->node_capacity = capacity;
    return MAELYS_JSON_OK;
}

static maelys_json_node_t *current_container(maelys_json_writer_t *writer) {
    return writer->depth ? &writer->nodes[writer->stack[writer->depth - 1u]] :
        NULL;
}

/* Appends a node to the current container (or as the root), taking the
 * pending key when the container is an object. Every failure is sticky. */
static maelys_json_result_t attach_node(
    maelys_json_writer_t *writer, maelys_json_type_t type, size_t *out_index) {
    maelys_json_node_t *container = current_container(writer);
    if (!container && writer->node_count) {
        return writer_fail(writer, MAELYS_JSON_ERR_STATE);
    }
    int needs_key = container && container->type == MAELYS_JSON_TYPE_OBJECT;
    if ((needs_key && !writer->pending_key) ||
        (!needs_key && writer->pending_key)) {
        return writer_fail(writer, MAELYS_JSON_ERR_STATE);
    }
    maelys_json_result_t result = grow_nodes(writer);
    if (result != MAELYS_JSON_OK) {
        return writer_fail(writer, result);
    }
    container = current_container(writer);
    size_t parent = container ? writer->stack[writer->depth - 1u] :
        MAELYS_JSON_VALUE_NONE;
    size_t index = writer->node_count++;
    maelys_json_node_t *node = &writer->nodes[index];
    *node = (maelys_json_node_t){.type = type};
    maelys_json_links_init(&node->links, parent);
    if (needs_key) {
        node->key = writer->pending_key;
        node->key_size = writer->pending_key_size;
        writer->pending_key = NULL;
        writer->pending_key_size = 0u;
        result = maelys_json_keyset_insert(&writer->keys, index);
        MAELYS_JSON_ASSERT(result != MAELYS_JSON_ERR_DUPLICATE_KEY);
        if (result != MAELYS_JSON_OK) {
            return writer_fail(writer, result);
        }
    }
    if (container) {
        maelys_json_links_t *last =
            container->links.last_child == MAELYS_JSON_VALUE_NONE ? NULL :
            &writer->nodes[container->links.last_child].links;
        maelys_json_links_append(&container->links, last, index);
    }
    *out_index = index;
    return MAELYS_JSON_OK;
}

maelys_json_result_t maelys_json_writer_create(
    maelys_json_profile_t profile, const maelys_json_limits_t *requested_limits,
    unsigned int flags, maelys_json_writer_t **out_writer) {
    const unsigned int known_flags = MAELYS_JSON_WRITER_FINAL_NEWLINE |
        MAELYS_JSON_WRITER_INDENT | MAELYS_JSON_WRITER_ASCII;
    if (!out_writer || !maelys_json_profile_valid(profile) ||
        (flags & ~known_flags)) {
        return MAELYS_JSON_ERR_ARGUMENT;
    }
    *out_writer = NULL;
    maelys_json_limits_t limits;
    if (maelys_json_limits_resolve(requested_limits, &limits) != MAELYS_JSON_OK) {
        return MAELYS_JSON_ERR_ARGUMENT;
    }
    maelys_json_writer_t *writer = calloc(1u, sizeof(*writer));
    if (!writer) {
        return MAELYS_JSON_ERR_MEMORY;
    }
    writer->profile = profile;
    writer->limits = limits;
    writer->flags = flags;
    writer->stack = malloc(limits.maximum_depth * sizeof(*writer->stack));
    if (!writer->stack || maelys_json_keyset_init(&writer->keys,
            WRITER_INITIAL_KEYS, node_key, writer) != MAELYS_JSON_OK) {
        maelys_json_writer_release(writer);
        return MAELYS_JSON_ERR_MEMORY;
    }
    *out_writer = writer;
    return MAELYS_JSON_OK;
}

void maelys_json_writer_release(maelys_json_writer_t *writer) {
    if (!writer) {
        return;
    }
    for (size_t i = 0u; i < writer->node_count; ++i) {
        free(writer->nodes[i].key);
        free(writer->nodes[i].string);
    }
    maelys_json_keyset_release(&writer->keys);
    free(writer->pending_key);
    free(writer->stack);
    free(writer->nodes);
    free(writer);
}

maelys_json_result_t maelys_json_writer_status(
    const maelys_json_writer_t *writer) {
    return writer ? writer->failure : MAELYS_JSON_ERR_ARGUMENT;
}

static maelys_json_result_t container_begin(
    maelys_json_writer_t *writer, maelys_json_type_t type) {
    maelys_json_result_t result = writer_ready(writer);
    if (result != MAELYS_JSON_OK) {
        return result;
    }
    if (writer->depth >= writer->limits.maximum_depth) {
        return writer_fail(writer, MAELYS_JSON_ERR_LIMIT);
    }
    size_t index = MAELYS_JSON_VALUE_NONE;
    result = attach_node(writer, type, &index);
    if (result != MAELYS_JSON_OK) {
        return result;
    }
    writer->stack[writer->depth++] = index;
    return MAELYS_JSON_OK;
}

static maelys_json_result_t container_end(
    maelys_json_writer_t *writer, maelys_json_type_t type) {
    maelys_json_result_t result = writer_ready(writer);
    if (result != MAELYS_JSON_OK) {
        return result;
    }
    maelys_json_node_t *container = current_container(writer);
    if (!container || container->type != type || writer->pending_key) {
        return writer_fail(writer, MAELYS_JSON_ERR_STATE);
    }
    --writer->depth;
    return MAELYS_JSON_OK;
}

maelys_json_result_t maelys_json_writer_object_begin(maelys_json_writer_t *writer) {
    return container_begin(writer, MAELYS_JSON_TYPE_OBJECT);
}

maelys_json_result_t maelys_json_writer_object_end(maelys_json_writer_t *writer) {
    return container_end(writer, MAELYS_JSON_TYPE_OBJECT);
}

maelys_json_result_t maelys_json_writer_array_begin(maelys_json_writer_t *writer) {
    return container_begin(writer, MAELYS_JSON_TYPE_ARRAY);
}

maelys_json_result_t maelys_json_writer_array_end(maelys_json_writer_t *writer) {
    return container_end(writer, MAELYS_JSON_TYPE_ARRAY);
}

maelys_json_result_t maelys_json_writer_key(
    maelys_json_writer_t *writer, const char *key, size_t size) {
    if (!writer || !valid_text(writer->profile, key, size)) {
        return MAELYS_JSON_ERR_ARGUMENT;
    }
    maelys_json_result_t result = writer_ready(writer);
    if (result != MAELYS_JSON_OK) {
        return result;
    }
    maelys_json_node_t *container = current_container(writer);
    if (!container || container->type != MAELYS_JSON_TYPE_OBJECT ||
        writer->pending_key) {
        return writer_fail(writer, MAELYS_JSON_ERR_STATE);
    }
    if (size > writer->limits.maximum_bytes) {
        return writer_fail(writer, MAELYS_JSON_ERR_LIMIT);
    }
    maelys_json_keyset_key_t candidate = {
        .parent = writer->stack[writer->depth - 1u], .bytes = key, .size = size
    };
    if (maelys_json_keyset_contains(&writer->keys, candidate)) {
        return writer_fail(writer, MAELYS_JSON_ERR_DUPLICATE_KEY);
    }
    writer->pending_key = copy_bytes(key, size);
    if (!writer->pending_key) {
        return writer_fail(writer, MAELYS_JSON_ERR_MEMORY);
    }
    writer->pending_key_size = size;
    return MAELYS_JSON_OK;
}

maelys_json_result_t maelys_json_writer_key_cstr(
    maelys_json_writer_t *writer, const char *key) {
    if (!key) {
        return MAELYS_JSON_ERR_ARGUMENT;
    }
    return maelys_json_writer_key(writer, key, strlen(key));
}

maelys_json_result_t maelys_json_writer_string(
    maelys_json_writer_t *writer, const char *value, size_t size) {
    if (!writer || !valid_text(writer->profile, value, size)) {
        return MAELYS_JSON_ERR_ARGUMENT;
    }
    maelys_json_result_t result = writer_ready(writer);
    if (result != MAELYS_JSON_OK) {
        return result;
    }
    if (size > writer->limits.maximum_bytes) {
        return writer_fail(writer, MAELYS_JSON_ERR_LIMIT);
    }
    char *copy = copy_bytes(value, size);
    if (!copy) {
        return writer_fail(writer, MAELYS_JSON_ERR_MEMORY);
    }
    size_t index;
    result = attach_node(writer, MAELYS_JSON_TYPE_STRING, &index);
    if (result != MAELYS_JSON_OK) {
        free(copy);
        return result;
    }
    writer->nodes[index].string = copy;
    writer->nodes[index].string_size = size;
    return MAELYS_JSON_OK;
}

maelys_json_result_t maelys_json_writer_string_cstr(
    maelys_json_writer_t *writer, const char *value) {
    if (!value) {
        return MAELYS_JSON_ERR_ARGUMENT;
    }
    return maelys_json_writer_string(writer, value, strlen(value));
}

static maelys_json_result_t attach_scalar(
    maelys_json_writer_t *writer, maelys_json_type_t type,
    maelys_json_node_t **out_node) {
    maelys_json_result_t result = writer_ready(writer);
    if (result != MAELYS_JSON_OK) {
        return result;
    }
    size_t index = MAELYS_JSON_VALUE_NONE;
    result = attach_node(writer, type, &index);
    if (result != MAELYS_JSON_OK) {
        return result;
    }
    *out_node = &writer->nodes[index];
    return MAELYS_JSON_OK;
}

maelys_json_result_t maelys_json_writer_u64(
    maelys_json_writer_t *writer, uint64_t value) {
    maelys_json_node_t *node;
    maelys_json_result_t result = attach_scalar(writer, MAELYS_JSON_TYPE_NUMBER,
        &node);
    if (result == MAELYS_JSON_OK) {
        node->unsigned_value = value;
    }
    return result;
}

maelys_json_result_t maelys_json_writer_i64(
    maelys_json_writer_t *writer, int64_t value) {
    maelys_json_node_t *node;
    maelys_json_result_t result = attach_scalar(writer, MAELYS_JSON_TYPE_NUMBER,
        &node);
    if (result == MAELYS_JSON_OK) {
        node->signed_value = value;
        node->number_signed = 1u;
    }
    return result;
}

maelys_json_result_t maelys_json_writer_boolean(
    maelys_json_writer_t *writer, int enabled) {
    maelys_json_node_t *node;
    maelys_json_result_t result = attach_scalar(writer, MAELYS_JSON_TYPE_BOOLEAN,
        &node);
    if (result == MAELYS_JSON_OK) {
        node->boolean_value = enabled != 0;
    }
    return result;
}

maelys_json_result_t maelys_json_writer_null(maelys_json_writer_t *writer) {
    maelys_json_node_t *node;
    return attach_scalar(writer, MAELYS_JSON_TYPE_NULL, &node);
}

static maelys_json_result_t copy_number(
    maelys_json_writer_t *writer, const maelys_json_document_t *document,
    maelys_json_value_t value) {
    maelys_json_view_t text;
    maelys_json_result_t result = maelys_json_value_number_text(document,
        value, &text);
    if (result != MAELYS_JSON_OK) {
        return result;
    }
    if (text.size && text.data[0] == '-') {
        int64_t number;
        result = maelys_json_value_i64(document, value, &number);
        return result == MAELYS_JSON_OK ?
            maelys_json_writer_i64(writer, number) : result;
    }
    uint64_t number;
    result = maelys_json_value_u64(document, value, &number);
    return result == MAELYS_JSON_OK ?
        maelys_json_writer_u64(writer, number) : result;
}

static maelys_json_result_t copy_value(
    maelys_json_writer_t *writer, const maelys_json_document_t *document,
    maelys_json_value_t value);

static maelys_json_result_t copy_object(
    maelys_json_writer_t *writer, const maelys_json_document_t *document,
    maelys_json_value_t object) {
    size_t count;
    maelys_json_result_t result = maelys_json_object_size(document, object,
        &count);
    if (result == MAELYS_JSON_OK) {
        result = maelys_json_writer_object_begin(writer);
    }
    for (size_t i = 0u; result == MAELYS_JSON_OK && i < count; ++i) {
        maelys_json_view_t key;
        maelys_json_value_t member;
        result = maelys_json_object_member_at(document, object, i, &key,
            &member);
        if (result == MAELYS_JSON_OK) {
            result = maelys_json_writer_key(writer, key.data, key.size);
        }
        if (result == MAELYS_JSON_OK) {
            result = copy_value(writer, document, member);
        }
    }
    return result == MAELYS_JSON_OK ?
        maelys_json_writer_object_end(writer) : result;
}

static maelys_json_result_t copy_array(
    maelys_json_writer_t *writer, const maelys_json_document_t *document,
    maelys_json_value_t array) {
    size_t count;
    maelys_json_result_t result = maelys_json_array_size(document, array,
        &count);
    if (result == MAELYS_JSON_OK) {
        result = maelys_json_writer_array_begin(writer);
    }
    for (size_t i = 0u; result == MAELYS_JSON_OK && i < count; ++i) {
        maelys_json_value_t element;
        result = maelys_json_array_get(document, array, i, &element);
        if (result == MAELYS_JSON_OK) {
            result = copy_value(writer, document, element);
        }
    }
    return result == MAELYS_JSON_OK ?
        maelys_json_writer_array_end(writer) : result;
}

static maelys_json_result_t copy_value(
    maelys_json_writer_t *writer, const maelys_json_document_t *document,
    maelys_json_value_t value) {
    maelys_json_view_t text;
    int enabled;
    maelys_json_result_t result;
    switch (maelys_json_value_type(document, value)) {
        case MAELYS_JSON_TYPE_OBJECT:
            return copy_object(writer, document, value);
        case MAELYS_JSON_TYPE_ARRAY:
            return copy_array(writer, document, value);
        case MAELYS_JSON_TYPE_STRING:
            result = maelys_json_value_string(document, value, &text);
            return result == MAELYS_JSON_OK ?
                maelys_json_writer_string(writer, text.data, text.size) : result;
        case MAELYS_JSON_TYPE_NUMBER:
            return copy_number(writer, document, value);
        case MAELYS_JSON_TYPE_BOOLEAN:
            result = maelys_json_value_boolean(document, value, &enabled);
            return result == MAELYS_JSON_OK ?
                maelys_json_writer_boolean(writer, enabled) : result;
        case MAELYS_JSON_TYPE_NULL:
            return maelys_json_writer_null(writer);
        case MAELYS_JSON_TYPE_NONE:
            break;
    }
    return MAELYS_JSON_ERR_ARGUMENT;
}

/* Turns a copy failure into the sticky state the failure model promises.
 * An ARGUMENT error found mid-copy (text the writer profile cannot carry)
 * leaves a partially built tree, so the writer cannot continue either. */
static maelys_json_result_t copy_failed(
    maelys_json_writer_t *writer, maelys_json_result_t result) {
    if (result == MAELYS_JSON_ERR_ARGUMENT) {
        writer_fail(writer, MAELYS_JSON_ERR_STATE);
        return result;
    }
    return writer_fail(writer, result);
}

maelys_json_result_t maelys_json_writer_value(
    maelys_json_writer_t *writer, const maelys_json_document_t *document,
    maelys_json_value_t value) {
    if (!writer || maelys_json_value_type(document, value) == MAELYS_JSON_TYPE_NONE) {
        return MAELYS_JSON_ERR_ARGUMENT;
    }
    maelys_json_result_t result = writer_ready(writer);
    if (result != MAELYS_JSON_OK) {
        return result;
    }
    result = copy_value(writer, document, value);
    return result == MAELYS_JSON_OK ? result : copy_failed(writer, result);
}

static int key_excluded(
    maelys_json_view_t key, const char *const *excluded_keys, size_t count) {
    for (size_t i = 0u; i < count; ++i) {
        if (strlen(excluded_keys[i]) == key.size &&
            memcmp(excluded_keys[i], key.data, key.size) == 0) {
            return 1;
        }
    }
    return 0;
}

maelys_json_result_t maelys_json_writer_object_begin_except(
    maelys_json_writer_t *writer, const maelys_json_document_t *document,
    maelys_json_value_t object, const char *const *excluded_keys,
    size_t excluded_count) {
    if (!writer || (!excluded_keys && excluded_count) ||
        maelys_json_value_type(document, object) != MAELYS_JSON_TYPE_OBJECT) {
        return MAELYS_JSON_ERR_ARGUMENT;
    }
    for (size_t i = 0u; i < excluded_count; ++i) {
        if (!excluded_keys[i]) {
            return MAELYS_JSON_ERR_ARGUMENT;
        }
    }
    maelys_json_result_t result = writer_ready(writer);
    if (result != MAELYS_JSON_OK) {
        return result;
    }
    result = maelys_json_writer_object_begin(writer);
    if (result != MAELYS_JSON_OK) {
        return result;
    }
    size_t count;
    result = maelys_json_object_size(document, object, &count);
    for (size_t i = 0u; result == MAELYS_JSON_OK && i < count; ++i) {
        maelys_json_view_t key;
        maelys_json_value_t member;
        result = maelys_json_object_member_at(document, object, i, &key, &member);
        if (result != MAELYS_JSON_OK ||
            key_excluded(key, excluded_keys, excluded_count)) {
            continue;
        }
        result = maelys_json_writer_key(writer, key.data, key.size);
        if (result == MAELYS_JSON_OK) {
            result = copy_value(writer, document, member);
        }
    }
    return result == MAELYS_JSON_OK ? result : copy_failed(writer, result);
}

maelys_json_result_t maelys_json_writer_finish(
    maelys_json_writer_t *writer, char **out_bytes, size_t *out_size) {
    if (!out_bytes || !out_size) {
        return MAELYS_JSON_ERR_ARGUMENT;
    }
    maelys_json_result_t result = writer_ready(writer);
    if (result != MAELYS_JSON_OK) {
        return result;
    }
    if (writer->depth || writer->pending_key || !writer->node_count) {
        return writer_fail(writer, MAELYS_JSON_ERR_STATE);
    }
    result = maelys_json_serialize(writer, out_bytes, out_size);
    if (result != MAELYS_JSON_OK) {
        return writer_fail(writer, result);
    }
    writer->finished = 1;
    return MAELYS_JSON_OK;
}

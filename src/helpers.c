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

/* SPDX-License-Identifier: MPL-2.0 */
/* Parses arbitrary bytes under both profiles and walks every value with the
 * typed readers, so the reader paths are covered too. */
#include "maelys/json.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

static void walk(const maelys_json_document_t *document, maelys_json_value_t value) {
    size_t count;
    maelys_json_view_t view;
    uint64_t u;
    int64_t i;
    int enabled;
    switch (maelys_json_value_type(document, value)) {
        case MAELYS_JSON_TYPE_OBJECT:
            if (maelys_json_object_size(document, value, &count) != MAELYS_JSON_OK) {
                abort();
            }
            for (size_t index = 0u; index < count; ++index) {
                maelys_json_value_t member;
                if (maelys_json_object_member_at(document, value, index, &view,
                        &member) != MAELYS_JSON_OK) {
                    abort();
                }
                maelys_json_value_t found;
                if (maelys_json_object_get_sized(document, value, view.data,
                        view.size, &found) != MAELYS_JSON_OK) {
                    abort();
                }
                walk(document, member);
            }
            break;
        case MAELYS_JSON_TYPE_ARRAY:
            if (maelys_json_array_size(document, value, &count) != MAELYS_JSON_OK) {
                abort();
            }
            for (size_t index = 0u; index < count; ++index) {
                maelys_json_value_t element;
                if (maelys_json_array_get(document, value, index, &element) !=
                    MAELYS_JSON_OK) {
                    abort();
                }
                walk(document, element);
            }
            break;
        case MAELYS_JSON_TYPE_STRING:
            if (maelys_json_value_string(document, value, &view) != MAELYS_JSON_OK ||
                view.data[view.size] != '\0') {
                abort();
            }
            break;
        case MAELYS_JSON_TYPE_NUMBER:
            if (maelys_json_value_number_text(document, value, &view) != MAELYS_JSON_OK) {
                abort();
            }
            (void)maelys_json_value_u64(document, value, &u);
            (void)maelys_json_value_i64(document, value, &i);
            break;
        case MAELYS_JSON_TYPE_BOOLEAN:
            if (maelys_json_value_boolean(document, value, &enabled) != MAELYS_JSON_OK) {
                abort();
            }
            break;
        case MAELYS_JSON_TYPE_NULL:
            if (!maelys_json_value_is_null(document, value)) {
                abort();
            }
            break;
        case MAELYS_JSON_TYPE_NONE:
            abort();
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    for (int profile = MAELYS_JSON_PROFILE_RFC8259;
         profile <= MAELYS_JSON_PROFILE_CONTRACT_ASCII; ++profile) {
        maelys_json_document_t *document = NULL;
        maelys_json_error_t error;
        maelys_json_result_t result = maelys_json_document_parse(data, size,
            (maelys_json_profile_t)profile, NULL, &document, &error);
        if ((result == MAELYS_JSON_OK) != (document != NULL) ||
            error.code != result) {
            abort();
        }
        if (result == MAELYS_JSON_OK) {
            walk(document, maelys_json_document_root(document));
            maelys_json_document_release(document);
        }
    }
    return 0;
}

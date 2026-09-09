/* SPDX-License-Identifier: MPL-2.0 */
/*
 * Canonical round trip: parse, serialize, parse again, serialize again. The
 * two canonical outputs must be identical (fixed point), and the indented
 * and ASCII presentation forms must parse back to the same canonical bytes.
 */
#include "maelys/json.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static char *serialize(const char *input, size_t size, unsigned int flags,
    size_t *out_size) {
    maelys_json_document_t *document = NULL;
    if (maelys_json_document_parse(input, size, MAELYS_JSON_PROFILE_RFC8259,
            NULL, &document, NULL) != MAELYS_JSON_OK) {
        return NULL;
    }
    maelys_json_writer_t *writer = NULL;
    if (maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, NULL, flags,
            &writer) != MAELYS_JSON_OK) {
        abort();
    }
    char *output = NULL;
    maelys_json_result_t result = maelys_json_writer_value(writer, document, 0u);
    if (result == MAELYS_JSON_OK) {
        result = maelys_json_writer_finish(writer, &output, out_size);
    }
    /* Non-integers and integers outside [-2^63, 2^64) cannot be
     * canonicalized; every other failure is a bug. */
    if (result != MAELYS_JSON_OK && result != MAELYS_JSON_ERR_NOT_INTEGER &&
        result != MAELYS_JSON_ERR_RANGE && result != MAELYS_JSON_ERR_LIMIT) {
        abort();
    }
    maelys_json_writer_release(writer);
    maelys_json_document_release(document);
    return output;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    size_t first_size;
    char *first = serialize((const char *)data, size, 0u, &first_size);
    if (!first) {
        return 0;
    }
    maelys_json_document_t *canonical_document = NULL;
    int canonical = 0;
    if (maelys_json_document_parse(first, first_size, MAELYS_JSON_PROFILE_RFC8259,
            NULL, &canonical_document, NULL) != MAELYS_JSON_OK ||
        maelys_json_document_is_canonical(canonical_document, 0u, &canonical) !=
            MAELYS_JSON_OK || !canonical) {
        abort();
    }
    maelys_json_document_release(canonical_document);
    static const unsigned int forms[] = {
        0u, MAELYS_JSON_WRITER_INDENT, MAELYS_JSON_WRITER_ASCII,
        MAELYS_JSON_WRITER_INDENT | MAELYS_JSON_WRITER_ASCII |
            MAELYS_JSON_WRITER_FINAL_NEWLINE
    };
    for (size_t i = 0u; i < sizeof(forms) / sizeof(forms[0]); ++i) {
        size_t form_size;
        char *form = serialize(first, first_size, forms[i], &form_size);
        if (!form) {
            abort();
        }
        size_t again_size;
        char *again = serialize(form, form_size, 0u, &again_size);
        if (!again || again_size != first_size ||
            memcmp(again, first, first_size) != 0) {
            abort();
        }
        free(again);
        free(form);
    }
    free(first);
    return 0;
}

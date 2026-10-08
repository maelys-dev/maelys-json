/* SPDX-License-Identifier: MPL-2.0 */
/*
 * Fuzzes the lexeme validator of maelys_json_writer_number_text against the
 * parser, the other reading of the `number` grammar of RFC 8259 section 6.
 * Two properties, in both directions:
 *
 * - every lexeme the parser kept for a number is accepted by the writer, up
 *   to the length bound the writer alone carries, and written back byte for
 *   byte;
 * - bytes the writer accepted parse as a number whose lexeme is those same
 *   bytes. The grammar carries no surrounding space, which a document may
 *   have, so the two are compared through the parsed lexeme and never
 *   through the raw input.
 */
#include "maelys/json.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void writes_back(const char *lexeme, size_t size) {
    maelys_json_writer_t *writer = NULL;
    if (maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, NULL, 0u,
            &writer) != MAELYS_JSON_OK) {
        abort();
    }
    maelys_json_result_t result = maelys_json_writer_number_text(writer, lexeme,
        size);
    if (result != MAELYS_JSON_OK) {
        /* The only refusal of a lexeme the parser accepted is its length. */
        if (result != MAELYS_JSON_ERR_ARGUMENT ||
                size <= MAELYS_JSON_MAXIMUM_NUMBER_TEXT) {
            abort();
        }
        maelys_json_writer_release(writer);
        return;
    }
    char *output = NULL;
    size_t output_size = 0u;
    if (maelys_json_writer_finish(writer, &output, &output_size) != MAELYS_JSON_OK) {
        abort();
    }
    maelys_json_writer_release(writer);
    if (output_size != size || memcmp(output, lexeme, size) != 0) {
        abort();
    }
    free(output);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 4096u) {
        return 0;
    }
    const char *lexeme = (const char *)data;
    maelys_json_writer_t *writer = NULL;
    if (maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, NULL, 0u,
            &writer) != MAELYS_JSON_OK) {
        abort();
    }
    maelys_json_result_t written = maelys_json_writer_number_text(writer, lexeme,
        size);
    if (written != MAELYS_JSON_OK && written != MAELYS_JSON_ERR_ARGUMENT) {
        abort();
    }
    maelys_json_writer_release(writer);

    maelys_json_document_t *document = NULL;
    maelys_json_result_t parsed = maelys_json_document_parse(lexeme, size,
        MAELYS_JSON_PROFILE_RFC8259, NULL, &document, NULL);
    int number = parsed == MAELYS_JSON_OK &&
        maelys_json_value_type(document, 0u) == MAELYS_JSON_TYPE_NUMBER;
    if (number) {
        maelys_json_view_t text;
        if (maelys_json_value_number_text(document, 0u, &text) != MAELYS_JSON_OK) {
            abort();
        }
        /* Bytes the writer took are a lexeme the parser reads identically. */
        if (written == MAELYS_JSON_OK &&
                (text.size != size || memcmp(text.data, lexeme, size) != 0)) {
            abort();
        }
        writes_back(text.data, text.size);
    } else if (written == MAELYS_JSON_OK) {
        /* The writer took bytes the parser does not read as a number. */
        abort();
    }
    maelys_json_document_release(document);
    return 0;
}

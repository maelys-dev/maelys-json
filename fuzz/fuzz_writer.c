/* SPDX-License-Identifier: MPL-2.0 */
/*
 * Drives the writer with an operation stream decoded from the input and
 * checks its failure model: after any non-ARGUMENT error every call reports
 * STATE, and a successful finish always produces parseable JSON.
 */
#include "maelys/json.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct stream {
    const uint8_t *data;
    size_t size;
    size_t offset;
} stream_t;

static int next_byte(stream_t *stream, uint8_t *out) {
    if (stream->offset >= stream->size) {
        return 0;
    }
    *out = stream->data[stream->offset++];
    return 1;
}

static maelys_json_result_t text_operation(
    maelys_json_writer_t *writer, stream_t *stream, int key) {
    /* An exhausted stream yields an empty text, so the writer is always
     * called and the failure model stays observable. */
    uint8_t length = 0u;
    (void)next_byte(stream, &length);
    size_t available = stream->size - stream->offset;
    size_t size = length < available ? length : available;
    const char *text = (const char *)stream->data + stream->offset;
    stream->offset += size;
    return key ? maelys_json_writer_key(writer, text, size) :
        maelys_json_writer_string(writer, text, size);
}

static maelys_json_result_t step(
    maelys_json_writer_t *writer, stream_t *stream, uint8_t operation) {
    uint64_t number = 0u;
    uint8_t byte;
    switch (operation % 11u) {
        case 0: return maelys_json_writer_object_begin(writer);
        case 1: return maelys_json_writer_object_end(writer);
        case 2: return maelys_json_writer_array_begin(writer);
        case 3: return maelys_json_writer_array_end(writer);
        case 4: return text_operation(writer, stream, 1);
        case 5: return text_operation(writer, stream, 0);
        case 6:
            for (int i = 0; i < 8 && next_byte(stream, &byte); ++i) {
                number = (number << 8u) | byte;
            }
            return maelys_json_writer_u64(writer, number);
        case 7:
            for (int i = 0; i < 8 && next_byte(stream, &byte); ++i) {
                number = (number << 8u) | byte;
            }
            return maelys_json_writer_i64(writer, (int64_t)number);
        case 8: return maelys_json_writer_boolean(writer, operation & 0x10u);
        case 9: return maelys_json_writer_null(writer);
        default: return maelys_json_writer_key_cstr(writer, "fixed");
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    stream_t stream = {.data = data, .size = size};
    uint8_t header;
    if (!next_byte(&stream, &header)) {
        return 0;
    }
    maelys_json_profile_t profile = (header & 1u) ?
        MAELYS_JSON_PROFILE_CONTRACT_ASCII : MAELYS_JSON_PROFILE_RFC8259;
    unsigned int flags = (header >> 1u) & 7u;
    maelys_json_limits_t limits = {
        .maximum_bytes = 4096u, .maximum_depth = 8u, .maximum_tokens = 64u
    };
    maelys_json_writer_t *writer = NULL;
    if (maelys_json_writer_create(profile, &limits, flags, &writer) != MAELYS_JSON_OK) {
        abort();
    }
    int failed = 0;
    uint8_t operation;
    while (next_byte(&stream, &operation)) {
        maelys_json_result_t result = step(writer, &stream, operation);
        if (failed && result != MAELYS_JSON_ERR_STATE &&
            result != MAELYS_JSON_ERR_ARGUMENT) {
            abort();
        }
        if (result != MAELYS_JSON_OK && result != MAELYS_JSON_ERR_ARGUMENT) {
            failed = 1;
        }
        if ((maelys_json_writer_status(writer) != MAELYS_JSON_OK) != failed) {
            abort();
        }
    }
    char *output = NULL;
    size_t output_size = 0u;
    maelys_json_result_t result = maelys_json_writer_finish(writer, &output,
        &output_size);
    if (result == MAELYS_JSON_OK) {
        if (failed || output[output_size] != '\0') {
            abort();
        }
        maelys_json_document_t *document = NULL;
        if (maelys_json_document_parse(output, output_size, profile, NULL,
                &document, NULL) != MAELYS_JSON_OK) {
            abort();
        }
        maelys_json_document_release(document);
        free(output);
    } else if (result != MAELYS_JSON_ERR_STATE && result != MAELYS_JSON_ERR_LIMIT) {
        abort();
    }
    maelys_json_writer_release(writer);
    return 0;
}

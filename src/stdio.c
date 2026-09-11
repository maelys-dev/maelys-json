/* SPDX-License-Identifier: MPL-2.0 */
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>

/* Reads at most capacity bytes; a short read at EOF is not an error. */
static maelys_json_result_t read_stream(
    FILE *stream, char *buffer, size_t capacity, size_t *out_size) {
    size_t size = 0u;
    while (size < capacity && !feof(stream) && !ferror(stream)) {
        size += fread(buffer + size, 1u, capacity - size, stream);
    }
    if (ferror(stream)) {
        return MAELYS_JSON_ERR_IO;
    }
    *out_size = size;
    return MAELYS_JSON_OK;
}

maelys_json_result_t maelys_json_document_parse_file_bytes(
    const char *path, maelys_json_profile_t profile,
    const maelys_json_limits_t *requested_limits,
    maelys_json_document_t **out_document, maelys_json_error_t *out_error,
    char **out_bytes, size_t *out_size) {
    if (out_error) {
        *out_error = (maelys_json_error_t){0};
    }
    maelys_json_limits_t limits;
    if (!path || !out_document || !out_bytes || !out_size ||
        !maelys_json_profile_valid(profile) ||
        maelys_json_limits_resolve(requested_limits, &limits) != MAELYS_JSON_OK) {
        maelys_json_error_set(out_error, MAELYS_JSON_ERR_ARGUMENT, NULL, 0u, 0u);
        return MAELYS_JSON_ERR_ARGUMENT;
    }
    *out_document = NULL;
    *out_bytes = NULL;
    *out_size = 0u;
    FILE *stream = fopen(path, "rb");
    if (!stream) {
        maelys_json_error_set(out_error, MAELYS_JSON_ERR_IO, NULL, 0u, 0u);
        return MAELYS_JSON_ERR_IO;
    }
    /* One extra byte detects an oversized file without reading it all. */
    size_t capacity = limits.maximum_bytes + 1u;
    char *buffer = malloc(capacity);
    if (!buffer) {
        (void)fclose(stream);
        maelys_json_error_set(out_error, MAELYS_JSON_ERR_MEMORY, NULL, 0u, 0u);
        return MAELYS_JSON_ERR_MEMORY;
    }
    size_t size;
    maelys_json_result_t result = read_stream(stream, buffer, capacity, &size);
    if (fclose(stream) != 0 && result == MAELYS_JSON_OK) {
        result = MAELYS_JSON_ERR_IO;
    }
    /* capacity is maximum_bytes + 1: filling it means the file is larger
     * than allowed, and otherwise the terminator below fits. */
    if (result == MAELYS_JSON_OK && size >= capacity) {
        result = MAELYS_JSON_ERR_LIMIT;
    }
    if (result != MAELYS_JSON_OK) {
        maelys_json_error_set(out_error, result, NULL, 0u, 0u);
        free(buffer);
        return result;
    }
    /* size <= maximum_bytes < capacity, so the terminator fits. */
    buffer[size] = '\0';
    result = maelys_json_document_parse(buffer, size, profile, &limits,
        out_document, out_error);
    *out_bytes = buffer;
    *out_size = size;
    return result;
}

maelys_json_result_t maelys_json_document_parse_file(
    const char *path, maelys_json_profile_t profile,
    const maelys_json_limits_t *limits, maelys_json_document_t **out_document,
    maelys_json_error_t *out_error) {
    char *bytes = NULL;
    size_t size = 0u;
    maelys_json_result_t result = maelys_json_document_parse_file_bytes(path,
        profile, limits, out_document, out_error, &bytes, &size);
    free(bytes);
    return result;
}

maelys_json_result_t maelys_json_writer_finish_file(
    maelys_json_writer_t *writer, FILE *stream) {
    if (!stream) {
        return MAELYS_JSON_ERR_ARGUMENT;
    }
    char *bytes;
    size_t size;
    maelys_json_result_t result = maelys_json_writer_finish(writer, &bytes,
        &size);
    if (result != MAELYS_JSON_OK) {
        return result;
    }
    size_t written = fwrite(bytes, 1u, size, stream);
    free(bytes);
    if (written != size || fflush(stream) != 0) {
        return MAELYS_JSON_ERR_IO;
    }
    return MAELYS_JSON_OK;
}

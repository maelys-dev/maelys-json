/* SPDX-License-Identifier: MPL-2.0 */
/*
 * maelys-json-canon: canonicalize, check or reformat one JSON document from
 * stdin to stdout. Development tool used by the JCS differential test.
 *
 *   maelys-json-canon [--contract] [--indent] [--ascii] [--newline] [--check]
 *
 * Exit codes: 0 success (with --check: canonical), 1 not canonical,
 * 2 parse error, 3 not canonicalizable (non-integer number), 4 usage.
 */
#include "maelys/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INPUT_LIMIT (16u * 1024u * 1024u)

static char *read_stdin(size_t *out_size) {
    size_t capacity = 65536u;
    size_t size = 0u;
    char *buffer = malloc(capacity);
    while (buffer) {
        size += fread(buffer + size, 1u, capacity - size, stdin);
        if (size < capacity || capacity >= INPUT_LIMIT) {
            break;
        }
        capacity *= 2u;
        char *grown = realloc(buffer, capacity);
        if (!grown) {
            free(buffer);
            buffer = NULL;
        } else {
            buffer = grown;
        }
    }
    *out_size = size;
    return buffer;
}

static int parse_arguments(int argc, char **argv, maelys_json_profile_t *profile,
    unsigned int *flags, int *check) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--contract") == 0) {
            *profile = MAELYS_JSON_PROFILE_CONTRACT_ASCII;
        } else if (strcmp(argv[i], "--indent") == 0) {
            *flags |= MAELYS_JSON_WRITER_INDENT;
        } else if (strcmp(argv[i], "--ascii") == 0) {
            *flags |= MAELYS_JSON_WRITER_ASCII;
        } else if (strcmp(argv[i], "--newline") == 0) {
            *flags |= MAELYS_JSON_WRITER_FINAL_NEWLINE;
        } else if (strcmp(argv[i], "--check") == 0) {
            *check = 1;
        } else {
            fprintf(stderr, "usage: maelys-json-canon [--contract] [--indent] "
                "[--ascii] [--newline] [--check]\n");
            return 0;
        }
    }
    return 1;
}

int main(int argc, char **argv) {
    maelys_json_profile_t profile = MAELYS_JSON_PROFILE_RFC8259;
    unsigned int flags = 0u;
    int check = 0;
    if (!parse_arguments(argc, argv, &profile, &flags, &check)) {
        return 4;
    }
    size_t size;
    char *input = read_stdin(&size);
    if (!input) {
        fputs("out of memory\n", stderr);
        return 2;
    }
    const maelys_json_limits_t limits = {
        .maximum_bytes = INPUT_LIMIT, .maximum_depth = MAELYS_JSON_MAXIMUM_DEPTH,
        .maximum_tokens = INPUT_LIMIT
    };
    maelys_json_document_t *document = NULL;
    maelys_json_error_t error;
    maelys_json_result_t result = maelys_json_document_parse(input, size,
        profile, &limits, &document, &error);
    if (result != MAELYS_JSON_OK) {
        char message[128];
        char pointer[256];
        maelys_json_error_format(&error, message, sizeof(message));
        maelys_json_error_pointer(input, size, &error, pointer, sizeof(pointer));
        fprintf(stderr, "%s at \"%s\"\n", message, pointer);
        free(input);
        return 2;
    }
    int status = 0;
    if (check) {
        int canonical = 0;
        result = maelys_json_document_is_canonical(document,
            flags & MAELYS_JSON_WRITER_FINAL_NEWLINE, &canonical);
        status = result != MAELYS_JSON_OK ? 2 : (canonical ? 0 : 1);
    } else {
        maelys_json_writer_t *writer = NULL;
        result = maelys_json_writer_create(profile, &limits, flags, &writer);
        if (result == MAELYS_JSON_OK) {
            result = maelys_json_writer_value(writer, document, 0u);
        }
        if (result == MAELYS_JSON_OK) {
            result = maelys_json_writer_finish_file(writer, stdout);
        }
        if (result != MAELYS_JSON_OK) {
            fprintf(stderr, "%s\n", maelys_json_result_string(result));
            status = result == MAELYS_JSON_ERR_NOT_INTEGER ||
                result == MAELYS_JSON_ERR_RANGE ? 3 : 2;
        }
        maelys_json_writer_release(writer);
    }
    maelys_json_document_release(document);
    free(input);
    return status;
}

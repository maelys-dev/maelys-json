/* SPDX-License-Identifier: MPL-2.0 */
/*
 * maelys-json bench: parse and canonicalize fixed synthetic inputs and
 * report nanoseconds per operation and throughput. Not a gate; compare runs
 * on the same machine to catch regressions.
 */
#include "maelys/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct input {
    const char *name;
    char *text;
    size_t size;
} input_t;

static double now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

static char *copy_text(const char *text) {
    size_t size = strlen(text) + 1u;
    char *copy = malloc(size);
    memcpy(copy, text, size);
    return copy;
}

static input_t make_config(void) {
    const char *text = "{\"name\":\"agent\",\"retries\":3,\"paths\":[\"/a\",\"/b\"],"
        "\"enabled\":true,\"limits\":{\"bytes\":65536,\"depth\":32}}";
    return (input_t){"config (100 B)", copy_text(text), strlen(text)};
}

static input_t make_int_array(void) {
    size_t capacity = 80000u;
    char *text = malloc(capacity);
    size_t size = 0u;
    text[size++] = '[';
    for (int i = 0; size < 64000u; ++i) {
        size += (size_t)snprintf(text + size, capacity - size, "%s%d", i ? "," : "",
            i * 7919);
    }
    text[size++] = ']';
    text[size] = '\0';
    return (input_t){"integer array (64 KiB)", text, size};
}

static input_t make_object(void) {
    size_t capacity = 32000u;
    char *text = malloc(capacity);
    size_t size = 0u;
    text[size++] = '{';
    for (int i = 0; i < 500; ++i) {
        size += (size_t)snprintf(text + size, capacity - size,
            "%s\"key%03d\":\"value \\u00e9 %d\"", i ? "," : "", 499 - i, i);
    }
    text[size++] = '}';
    text[size] = '\0';
    return (input_t){"object, 500 keys (14 KiB)", text, size};
}

static input_t make_deep(void) {
    size_t depth = 200u;
    char *text = malloc(2u * depth + 1u);
    memset(text, '[', depth);
    memset(text + depth, ']', depth);
    text[2u * depth] = '\0';
    return (input_t){"nesting, depth 200", text, 2u * depth};
}

static input_t make_strings(void) {
    size_t capacity = 40000u;
    char *text = malloc(capacity);
    size_t size = 0u;
    text[size++] = '[';
    for (int i = 0; size < 32000u; ++i) {
        size += (size_t)snprintf(text + size, capacity - size,
            "%s\"line %d \\n \\\"quoted\\\" \\u20ac caf\xc3\xa9 \\ud83d\\ude80\"",
            i ? "," : "", i);
    }
    text[size++] = ']';
    text[size] = '\0';
    return (input_t){"strings with escapes (32 KiB)", text, size};
}

static void run(const input_t *input) {
    const maelys_json_limits_t limits = {
        .maximum_bytes = 1u << 20, .maximum_depth = MAELYS_JSON_MAXIMUM_DEPTH,
        .maximum_tokens = 1u << 18
    };
    int iterations = input->size < 1000u ? 200000 : 2000;
    double start = now_ns();
    for (int i = 0; i < iterations; ++i) {
        maelys_json_document_t *document;
        if (maelys_json_document_parse(input->text, input->size,
                MAELYS_JSON_PROFILE_RFC8259, &limits, &document, NULL) != MAELYS_JSON_OK) {
            fprintf(stderr, "%s: parse failed\n", input->name);
            exit(1);
        }
        maelys_json_document_release(document);
    }
    double parse_ns = (now_ns() - start) / iterations;
    maelys_json_document_t *document;
    maelys_json_document_parse(input->text, input->size, MAELYS_JSON_PROFILE_RFC8259,
        &limits, &document, NULL);
    start = now_ns();
    for (int i = 0; i < iterations; ++i) {
        maelys_json_writer_t *writer;
        char *bytes;
        size_t size;
        maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, &limits, 0u, &writer);
        if (maelys_json_writer_value(writer, document, 0u) != MAELYS_JSON_OK ||
            maelys_json_writer_finish(writer, &bytes, &size) != MAELYS_JSON_OK) {
            fprintf(stderr, "%s: canonicalize failed\n", input->name);
            exit(1);
        }
        free(bytes);
        maelys_json_writer_release(writer);
    }
    double write_ns = (now_ns() - start) / iterations;
    maelys_json_document_release(document);
    printf("%-32s parse %10.0f ns  %7.1f MB/s   canonicalize %10.0f ns\n",
        input->name, parse_ns, (double)input->size / parse_ns * 1e3, write_ns);
}

int main(void) {
    input_t inputs[] = {
        make_config(), make_int_array(), make_object(), make_deep(), make_strings()
    };
    printf("maelys-json %s bench\n", maelys_json_version());
    for (size_t i = 0u; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
        run(&inputs[i]);
        free(inputs[i].text);
    }
    return 0;
}

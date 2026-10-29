/* SPDX-License-Identifier: MPL-2.0 */
/*
 * The canonical check, held against the implementation it replaces.
 *
 * `reference_is_canonical` is the algorithm maelys-json carried until the
 * streaming check: copy the whole document into a writer, serialize it, and
 * compare the bytes, with a prior walk ruling out a number the canonical
 * numeric domain does not hold. It is written here with the public API
 * alone, so the two answers can be compared on the same input for as long
 * as this file lives, rather than once in a pull request.
 *
 * The differential runs over the canonical vectors, the JSONTestSuite
 * corpus, the fuzz seed corpus, and a table of variants aimed at the places
 * the two could disagree: whitespace, escape spellings, key order including
 * keys outside the basic plane, the integer bounds, `-0`, and the final
 * newline under both flag settings.
 */
#include "framework.h"
#include "reference_canonical.h"

#include <dirent.h>
#include <stdlib.h>

/* Parses `text` in `profile` and compares the two answers under both flag
 * settings. Returns 1 on a disagreement, 0 otherwise, and says nothing
 * about inputs the parser refuses. */
static int differ(const char *text, size_t size, maelys_json_profile_t profile,
    const char *origin) {
    static const maelys_json_limits_t limits = {
        .maximum_bytes = 1u << 20, .maximum_depth = MAELYS_JSON_MAXIMUM_DEPTH,
        .maximum_tokens = 1u << 18
    };
    maelys_json_document_t *document = NULL;
    if (maelys_json_document_parse(text, size, profile, &limits, &document,
            NULL) != MAELYS_JSON_OK) {
        return 0;
    }
    static const unsigned int settings[] = {0u, MAELYS_JSON_WRITER_FINAL_NEWLINE};
    int failures = 0;
    for (size_t i = 0u; i < sizeof(settings) / sizeof(settings[0]); ++i) {
        int mine = -1;
        int theirs = -2;
        maelys_json_result_t my_result = maelys_json_document_is_canonical(
            document, settings[i], &mine);
        maelys_json_result_t their_result = reference_is_canonical(document,
            text, size, profile, settings[i], &theirs);
        if (my_result != their_result || mine != theirs) {
            fprintf(stderr,
                "    %s (profile %d, flags %u): status %s vs %s, answer %d vs %d\n",
                origin, (int)profile, settings[i],
                maelys_json_result_string(my_result),
                maelys_json_result_string(their_result), mine, theirs);
            ++failures;
        }
    }
    maelys_json_document_release(document);
    return failures;
}

static int differ_all_profiles(
    const char *text, size_t size, const char *origin) {
    return differ(text, size, MAELYS_JSON_PROFILE_RFC8259, origin) +
        differ(text, size, MAELYS_JSON_PROFILE_CONTRACT_ASCII, origin) +
        differ(text, size, MAELYS_JSON_PROFILE_RFC8259_LAST_WINS, origin);
}

/* Inputs chosen where the two implementations could part company. */
static int variants(void) {
    static const char *const inputs[] = {
        /* Last-wins survivors can be canonical while their source is not. */
        "{\"id\":1,\"id\":1}",
        "{\"id\":1,\"id\":2}\n",
        "{\"id\":1,\"\\u0069d\":2}",
        "{\"x\":{\"a\":1,\"a\":2},\"x\":0}",
        "{\"a\":1.5,\"a\":0}",
        "{\"a\":0,\"a\":1.5}",
        /* Shape and whitespace. */
        "{}",
        "[]",
        "1",
        "0",
        "-1",
        "\"\"",
        "true",
        "false",
        "null",
        "{\"a\":1}",
        "{ \"a\":1}",
        "{\"a\": 1}",
        "{\"a\":1 }",
        "[1, 2]",
        "[1,2]\n",
        "{\"a\":[]}",
        "\n{\"a\":1}",
        "[[[1]]]",
        /* Key order, including keys outside the basic multilingual plane,
         * where UTF-8 and UTF-16 orders differ. */
        "{\"a\":1,\"b\":2}",
        "{\"b\":1,\"a\":2}",
        "{\"\xef\xbd\xa1\":1,\"\xf0\x90\x80\x80\":2}",
        "{\"\xf0\x90\x80\x80\":1,\"\xef\xbd\xa1\":2}",
        "{\"\xee\x80\x80\":1,\"\xf0\x9f\x98\x80\":2}",
        "{\"\xf0\x9f\x98\x80\":1,\"\xee\x80\x80\":2}",
        "{\"a\":1,\"ab\":2}",
        "{\"ab\":1,\"a\":2}",
        /* Escape spellings: only the canonical one is canonical. */
        "\"A\"",
        "\"\\u0041\"",
        "\"\\/\"",
        "\"/\"",
        "\"\\\"\"",
        "\"\\\\\"",
        "\"\\b\\f\\n\\r\\t\"",
        "\"\\u0000\"",
        "\"\\u0001\"",
        "\"\\u001f\"",
        "\"\\u001F\"",
        "\"\\u007f\"",
        "\"\x7f\"",
        "\"\xc3\xa9\"",
        "\"\\u00e9\"",
        "\"\xf0\x9f\x98\x80\"",
        "\"\\ud83d\\ude00\"",
        /* Numbers: the integer bounds, -0, fractions and exponents. */
        "-0",
        "0",
        "[-0]",
        "{\"a\":-0}",
        "9223372036854775807",
        "-9223372036854775808",
        "18446744073709551615",
        "18446744073709551616",
        "-9223372036854775809",
        "99999999999999999999999999",
        "1.5",
        "1e2",
        "1E2",
        "1.0",
        "0.0",
        "-1.5e-3",
        "1e400",
        "[1,1.5]",
        "{\"a\":1,\"b\":2.5}",
    };
    int failures = 0;
    for (size_t i = 0u; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
        failures +=
            differ_all_profiles(inputs[i], strlen(inputs[i]), inputs[i]);
    }
    /* A document at the depth ceiling, built rather than written out. */
    size_t depth = MAELYS_JSON_MAXIMUM_DEPTH;
    char *nested = malloc(2u * depth + 1u);
    CHECK(nested != NULL);
    for (size_t i = 0u; i < depth; ++i) {
        nested[i] = '[';
        nested[2u * depth - 1u - i] = ']';
    }
    nested[2u * depth] = '\0';
    failures += differ_all_profiles(nested, 2u * depth, "deepest array");
    free(nested);
    CHECK(failures == 0);
    return 0;
}

static char *read_whole(const char *path, size_t *out_size) {
    FILE *stream = fopen(path, "rb");
    if (!stream) {
        return NULL;
    }
    char *buffer = NULL;
    size_t size = 0u;
    size_t capacity = 0u;
    for (;;) {
        if (size == capacity) {
            capacity = capacity ? capacity * 2u : 4096u;
            char *grown = realloc(buffer, capacity + 1u);
            if (!grown) {
                free(buffer);
                fclose(stream);
                return NULL;
            }
            buffer = grown;
        }
        size_t chunk = fread(buffer + size, 1u, capacity - size, stream);
        if (!chunk) {
            break;
        }
        size += chunk;
    }
    fclose(stream);
    buffer[size] = '\0';
    *out_size = size;
    return buffer;
}

/* Every regular file of `directory` whose name ends in `suffix`, or every
 * file when `suffix` is NULL. */
static int differ_directory(const char *directory, const char *suffix,
    int *out_files) {
    DIR *handle = opendir(directory);
    if (!handle) {
        fprintf(stderr, "    cannot open %s\n", directory);
        return -1;
    }
    int failures = 0;
    struct dirent *entry;
    while ((entry = readdir(handle)) != NULL) {
        const char *name = entry->d_name;
        size_t length = strlen(name);
        if (name[0] == '.' || (suffix && (length < strlen(suffix) ||
                strcmp(name + length - strlen(suffix), suffix) != 0))) {
            continue;
        }
        char path[1024];
        if ((size_t)snprintf(path, sizeof(path), "%s/%s", directory, name) >=
                sizeof(path)) {
            continue;
        }
        size_t size = 0u;
        char *bytes = read_whole(path, &size);
        if (!bytes) {
            continue;
        }
        ++*out_files;
        failures += differ_all_profiles(bytes, size, name);
        free(bytes);
    }
    closedir(handle);
    return failures;
}

static int corpora(void) {
    int files = 0;
    int failures = 0;
    char directory[1024];
    int written = snprintf(directory, sizeof(directory), "%s",
        test_vectors_directory());
    CHECK(written > 0 && (size_t)written < sizeof(directory));
    int result = differ_directory(directory, NULL, &files);
    CHECK(result >= 0);
    failures += result;
    result = differ_directory(test_suite_directory(), ".json", &files);
    CHECK(result >= 0);
    failures += result;
    /* The fuzz seed corpora live beside the harnesses. */
    static const char *const fuzz[] = {
        "tests/fuzz/corpus", "tests/fuzz/corpus-number-text"
    };
    for (size_t i = 0u; i < sizeof(fuzz) / sizeof(fuzz[0]); ++i) {
        DIR *probe = opendir(fuzz[i]);
        if (!probe) {
            continue;
        }
        closedir(probe);
        result = differ_directory(fuzz[i], NULL, &files);
        CHECK(result >= 0);
        failures += result;
    }
    printf("    differential: %d files, %d disagreements\n", files, failures);
    CHECK(files > 0 && failures == 0);
    return 0;
}

static const test_case_t cases[] = {
    {"variants", variants},
    {"corpora", corpora},
};

TEST_SUITE(test_canonical_suite, cases)

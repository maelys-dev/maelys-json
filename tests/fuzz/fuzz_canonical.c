/* SPDX-License-Identifier: MPL-2.0 */
/*
 * Holds the streaming canonical check against the implementation it
 * replaced, on whatever the parser accepts: same status and same answer,
 * under both flag settings and both profiles. The reference copies the
 * document into a writer and compares the serialized bytes, which is what
 * this check must agree with for ever, and never with a second reading of
 * the format written here.
 */
#include "../reference_canonical.h"

#include <stdint.h>
#include <stdio.h>

static void compare(const char *text, size_t size,
    maelys_json_profile_t profile) {
    static const maelys_json_limits_t limits = {
        .maximum_bytes = 1u << 16, .maximum_depth = 32u,
        .maximum_tokens = 1u << 14
    };
    maelys_json_document_t *document = NULL;
    if (maelys_json_document_parse(text, size, profile, &limits, &document,
            NULL) != MAELYS_JSON_OK) {
        return;
    }
    static const unsigned int settings[] = {0u, MAELYS_JSON_WRITER_FINAL_NEWLINE};
    for (size_t i = 0u; i < sizeof(settings) / sizeof(settings[0]); ++i) {
        int mine = -1;
        int theirs = -2;
        maelys_json_result_t my_result = maelys_json_document_is_canonical(
            document, settings[i], &mine);
        maelys_json_result_t their_result = reference_is_canonical(document,
            text, size, profile, settings[i], &theirs);
        if (my_result != their_result || mine != theirs) {
            fprintf(stderr, "flags %u: status %s vs %s, answer %d vs %d\n",
                settings[i], maelys_json_result_string(my_result),
                maelys_json_result_string(their_result), mine, theirs);
            abort();
        }
    }
    maelys_json_document_release(document);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 65536u) {
        return 0;
    }
    compare((const char *)data, size, MAELYS_JSON_PROFILE_RFC8259);
    compare((const char *)data, size, MAELYS_JSON_PROFILE_CONTRACT_ASCII);
    return 0;
}

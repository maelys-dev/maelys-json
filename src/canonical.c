/* SPDX-License-Identifier: MPL-2.0 */
/*
 * Decides whether the bytes of a parsed document are already Maelys
 * Canonical JSON v1 (docs/canonical-json-v1.md).
 *
 * The question is answered against the input itself: a cursor walks the
 * document and demands, byte by byte, exactly what the canonical writer
 * would have produced, stopping at the first disagreement. Nothing is
 * copied and nothing is serialized, so the auxiliary space is one cursor
 * and the recursion, bounded by the depth the parse already accepted;
 * before this, the check built a whole writer copy of the document and a
 * whole output buffer to compare with, for a yes or no answer.
 *
 * The two rules the canonical form rests on are shared with the writer and
 * live elsewhere: the spelling of a byte inside a string literal
 * (maelys_json_canonical_escape) and the order of object keys
 * (maelys_json_utf16_compare). A number is rendered by the same snprintf
 * the serializer uses. So this file decides where the bytes must sit, not
 * what they must be.
 */
#include "internal.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

typedef struct cursor {
    const char *bytes;
    size_t size;
    size_t offset;
} cursor_t;

/* Each `expect` consumes what the canonical form requires here, or answers
 * 0 and leaves the walk to unwind: a mismatch is not an error, it is the
 * answer "not canonical". */
static int expect(cursor_t *cursor, const char *text, size_t size) {
    if (cursor->size - cursor->offset < size ||
        memcmp(cursor->bytes + cursor->offset, text, size) != 0) {
        return 0;
    }
    cursor->offset += size;
    return 1;
}

static int expect_byte(cursor_t *cursor, char byte) {
    return expect(cursor, &byte, 1u);
}

static int expect_literal(cursor_t *cursor, const char *text) {
    return expect(cursor, text, strlen(text));
}

/* A string literal: the quotation marks, and every byte of the decoded text
 * in its canonical spelling. A document that wrote `A` for `A`, or
 * `\/`, disagrees here. */
static int expect_string(cursor_t *cursor, const char *text, size_t size) {
    if (!expect_byte(cursor, '"')) {
        return 0;
    }
    for (size_t i = 0u; i < size; ++i) {
        char escaped[6];
        size_t length = maelys_json_canonical_escape((unsigned char)text[i],
            escaped);
        if (!expect(cursor, escaped, length)) {
            return 0;
        }
    }
    return expect_byte(cursor, '"');
}

/* A number: integral, in [-2^63, 2^64 - 1], and spelled as the serializer
 * spells it. A fraction or an exponent is outside the canonical numeric
 * domain; `-0` renders as `0` and so disagrees with its own source. */
static int expect_number(
    cursor_t *cursor, const maelys_json_document_t *document,
    maelys_json_value_t value, const maelys_json_token_t *token) {
    for (size_t i = 0u; i < token->text_size; ++i) {
        char byte = token->text[i];
        if (byte == '.' || byte == 'e' || byte == 'E') {
            return 0;
        }
    }
    char rendered[32];
    int length;
    if (token->text_size != 0u && token->text[0] == '-') {
        int64_t number;
        if (maelys_json_value_i64(document, value, &number) != MAELYS_JSON_OK) {
            return 0;
        }
        length = snprintf(rendered, sizeof(rendered), "%" PRId64, number);
    } else {
        uint64_t number;
        if (maelys_json_value_u64(document, value, &number) != MAELYS_JSON_OK) {
            return 0;
        }
        length = snprintf(rendered, sizeof(rendered), "%" PRIu64, number);
    }
    MAELYS_JSON_ASSERT(length > 0 && (size_t)length < sizeof(rendered));
    return length > 0 && expect(cursor, rendered, (size_t)length);
}

static int expect_value(
    cursor_t *cursor, const maelys_json_document_t *document,
    maelys_json_value_t value);

/* An object: no whitespace, members in UTF-16 key order, one colon and one
 * comma where the form puts them. Keys are compared strictly, so a pair of
 * equal keys would disagree too; the parser has already refused those. */
static int expect_object(
    cursor_t *cursor, const maelys_json_document_t *document,
    const maelys_json_token_t *token) {
    if (!expect_byte(cursor, '{')) {
        return 0;
    }
    size_t members = token->links.child_count / 2u;
    const char *previous_key = NULL;
    size_t previous_size = 0u;
    for (size_t i = 0u; i < members; ++i) {
        if (i != 0u && !expect_byte(cursor, ',')) {
            return 0;
        }
        size_t key_index = document->children[token->children_offset + 2u * i];
        const maelys_json_token_t *key = &document->tokens[key_index];
        MAELYS_JSON_ASSERT(key->object_key);
        if (previous_key && maelys_json_utf16_compare(previous_key,
                previous_size, key->text, key->text_size) >= 0) {
            return 0;
        }
        if (!expect_string(cursor, key->text, key->text_size) ||
            !expect_byte(cursor, ':') ||
            !expect_value(cursor, document,
                document->children[token->children_offset + 2u * i + 1u])) {
            return 0;
        }
        previous_key = key->text;
        previous_size = key->text_size;
    }
    return expect_byte(cursor, '}');
}

static int expect_array(
    cursor_t *cursor, const maelys_json_document_t *document,
    const maelys_json_token_t *token) {
    if (!expect_byte(cursor, '[')) {
        return 0;
    }
    for (size_t i = 0u; i < token->links.child_count; ++i) {
        if (i != 0u && !expect_byte(cursor, ',')) {
            return 0;
        }
        if (!expect_value(cursor, document,
                document->children[token->children_offset + i])) {
            return 0;
        }
    }
    return expect_byte(cursor, ']');
}

static int expect_value(
    cursor_t *cursor, const maelys_json_document_t *document,
    maelys_json_value_t value) {
    MAELYS_JSON_ASSERT(value < document->token_count);
    const maelys_json_token_t *token = &document->tokens[value];
    switch (token->type) {
        case MAELYS_JSON_TYPE_OBJECT:
            return expect_object(cursor, document, token);
        case MAELYS_JSON_TYPE_ARRAY:
            return expect_array(cursor, document, token);
        case MAELYS_JSON_TYPE_STRING:
            return expect_string(cursor, token->text, token->text_size);
        case MAELYS_JSON_TYPE_NUMBER:
            return expect_number(cursor, document, value, token);
        case MAELYS_JSON_TYPE_BOOLEAN:
            return expect_literal(cursor, token->boolean_value ? "true" : "false");
        case MAELYS_JSON_TYPE_NULL:
            return expect_literal(cursor, "null");
        case MAELYS_JSON_TYPE_NONE:
            break;
    }
    /* Only MAELYS_JSON_TYPE_NONE reaches this point, and no token has it. */
    MAELYS_JSON_ASSERT(token->type != MAELYS_JSON_TYPE_NONE);
    return 0;
}

maelys_json_result_t maelys_json_document_is_canonical(
    const maelys_json_document_t *document, unsigned int flags,
    int *out_canonical) {
    if (!document || !out_canonical ||
        (flags & ~MAELYS_JSON_WRITER_FINAL_NEWLINE)) {
        return MAELYS_JSON_ERR_ARGUMENT;
    }
    if (document->duplicate_count) {
        *out_canonical = 0;
        return MAELYS_JSON_OK;
    }
    cursor_t cursor = {document->bytes, document->size, 0u};
    int canonical = document->token_count != 0u &&
        expect_value(&cursor, document, 0u);
    if (canonical && (flags & MAELYS_JSON_WRITER_FINAL_NEWLINE)) {
        canonical = expect_byte(&cursor, '\n');
    }
    /* Leading or trailing bytes the walk did not claim, whitespace included,
     * are not part of the canonical form. */
    *out_canonical = canonical && cursor.offset == document->size;
    return MAELYS_JSON_OK;
}

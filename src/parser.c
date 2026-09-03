#include "internal.h"
#include "keyset.h"

#include <stdlib.h>
#include <string.h>

typedef struct parser {
    maelys_json_document_t *document;
    maelys_json_profile_t profile;
    maelys_json_limits_t limits;
    maelys_json_error_t *error;
    size_t offset;
    maelys_json_keyset_t keys;
} parser_t;

static maelys_json_keyset_key_t token_key(const void *context, size_t index) {
    const maelys_json_document_t *document = context;
    const maelys_json_token_t *token = &document->tokens[index];
    return (maelys_json_keyset_key_t){
        .parent = token->links.parent, .bytes = token->text,
        .size = token->text_size
    };
}

static maelys_json_result_t fail(
    parser_t *parser, maelys_json_result_t result, size_t offset) {
    maelys_json_error_set(parser->error, result, parser->document->bytes,
        parser->document->size, offset);
    return result;
}

static int is_space(char byte) {
    return byte == ' ' || byte == '\t' || byte == '\r' || byte == '\n';
}

static int at_end(const parser_t *parser) {
    return parser->offset >= parser->document->size;
}

static char current(const parser_t *parser) {
    return parser->document->bytes[parser->offset];
}

static void skip_space(parser_t *parser) {
    while (!at_end(parser) && is_space(current(parser))) {
        ++parser->offset;
    }
}

/* Hands out `capacity` bytes of the arena. The arena is sized so that every
 * decoded text plus its NUL fits; running out is a library bug. */
static char *arena_claim(maelys_json_document_t *document, size_t capacity) {
    (void)capacity; /* only checked by the assertion */
    MAELYS_JSON_ASSERT(document->arena_used + capacity <=
        document->arena_capacity);
    return document->arena + document->arena_used;
}

static maelys_json_result_t add_token(
    parser_t *parser, maelys_json_type_t type, size_t start, size_t parent,
    unsigned char object_key, size_t *out_index) {
    maelys_json_document_t *document = parser->document;
    if (document->token_count >= document->token_capacity) {
        return fail(parser, MAELYS_JSON_ERR_LIMIT, start);
    }
    size_t index = document->token_count++;
    maelys_json_token_t *token = &document->tokens[index];
    *token = (maelys_json_token_t){
        .type = type, .start = start, .end = start, .object_key = object_key
    };
    maelys_json_links_init(&token->links, parent);
    if (parent != MAELYS_JSON_VALUE_NONE) {
        maelys_json_links_t *container = &document->tokens[parent].links;
        maelys_json_links_t *last = container->last_child == MAELYS_JSON_VALUE_NONE ?
            NULL : &document->tokens[container->last_child].links;
        maelys_json_links_append(container, last, index);
    }
    *out_index = index;
    return MAELYS_JSON_OK;
}

static maelys_json_result_t scan_string_body(parser_t *parser, size_t *out_end) {
    while (!at_end(parser)) {
        unsigned char byte = (unsigned char)current(parser);
        if (byte == '"') {
            *out_end = parser->offset++;
            return MAELYS_JSON_OK;
        }
        if (byte < 0x20u) {
            return fail(parser, MAELYS_JSON_ERR_SYNTAX, parser->offset);
        }
        if (parser->profile == MAELYS_JSON_PROFILE_CONTRACT_ASCII &&
            (byte == '\\' || byte > 0x7eu)) {
            return fail(parser, MAELYS_JSON_ERR_SYNTAX, parser->offset);
        }
        if (byte == '\\') {
            ++parser->offset;
            if (at_end(parser)) {
                break;
            }
        }
        ++parser->offset;
    }
    return MAELYS_JSON_ERR_SYNTAX;
}

static maelys_json_result_t parse_string(
    parser_t *parser, size_t parent, unsigned char object_key,
    size_t *out_index) {
    size_t opening = parser->offset;
    if (at_end(parser) || current(parser) != '"') {
        return fail(parser, MAELYS_JSON_ERR_SYNTAX, opening);
    }
    size_t start = ++parser->offset;
    size_t end = 0u;
    maelys_json_result_t result = scan_string_body(parser, &end);
    if (result != MAELYS_JSON_OK) {
        return parser->error && parser->error->code ? result :
            fail(parser, MAELYS_JSON_ERR_SYNTAX, opening);
    }
    size_t index;
    result = add_token(parser, MAELYS_JSON_TYPE_STRING, start, parent,
        object_key, &index);
    if (result != MAELYS_JSON_OK) {
        return result;
    }
    maelys_json_document_t *document = parser->document;
    maelys_json_token_t *token = &document->tokens[index];
    token->end = end;
    char *text = arena_claim(document, end - start + 1u);
    size_t text_size;
    size_t bad = 0u;
    result = maelys_json_decode_string(document->bytes + start, end - start,
        parser->profile, text, &text_size, &bad);
    if (result != MAELYS_JSON_OK) {
        return fail(parser, result, start + bad);
    }
    token->text = text;
    token->text_size = text_size;
    document->arena_used += text_size + 1u;
    if (object_key) {
        result = maelys_json_keyset_insert(&parser->keys, index);
        if (result != MAELYS_JSON_OK) {
            return fail(parser, result, opening);
        }
    }
    *out_index = index;
    return MAELYS_JSON_OK;
}

static maelys_json_result_t parse_value(
    parser_t *parser, size_t parent, size_t depth, size_t *out_index);

/* Consumes ',' or the closing delimiter. Returns 1 when the container is
 * closed, 0 to continue, or sets *out_result on error. */
static int container_delimiter(
    parser_t *parser, char closing, maelys_json_result_t *out_result) {
    skip_space(parser);
    if (at_end(parser)) {
        *out_result = fail(parser, MAELYS_JSON_ERR_SYNTAX, parser->offset);
        return 1;
    }
    size_t at = parser->offset++;
    char delimiter = parser->document->bytes[at];
    if (delimiter == closing) {
        *out_result = MAELYS_JSON_OK;
        return 1;
    }
    if (delimiter != ',') {
        *out_result = fail(parser, MAELYS_JSON_ERR_SYNTAX, at);
        return 1;
    }
    skip_space(parser);
    return 0;
}

static maelys_json_result_t parse_object(
    parser_t *parser, size_t parent, size_t depth, size_t *out_index) {
    if (depth >= parser->limits.maximum_depth) {
        return fail(parser, MAELYS_JSON_ERR_LIMIT, parser->offset);
    }
    size_t object;
    maelys_json_result_t result = add_token(parser, MAELYS_JSON_TYPE_OBJECT,
        parser->offset, parent, 0u, &object);
    if (result != MAELYS_JSON_OK) {
        return result;
    }
    ++parser->offset;
    skip_space(parser);
    if (!at_end(parser) && current(parser) == '}') {
        ++parser->offset;
    } else {
        for (;;) {
            size_t key;
            result = parse_string(parser, object, 1u, &key);
            if (result != MAELYS_JSON_OK) {
                return result;
            }
            skip_space(parser);
            if (at_end(parser) || current(parser) != ':') {
                return fail(parser, MAELYS_JSON_ERR_SYNTAX, parser->offset);
            }
            ++parser->offset;
            size_t value;
            result = parse_value(parser, object, depth + 1u, &value);
            if (result != MAELYS_JSON_OK) {
                return result;
            }
            if (container_delimiter(parser, '}', &result)) {
                break;
            }
        }
        if (result != MAELYS_JSON_OK) {
            return result;
        }
    }
    parser->document->tokens[object].end = parser->offset;
    *out_index = object;
    return MAELYS_JSON_OK;
}

static maelys_json_result_t parse_array(
    parser_t *parser, size_t parent, size_t depth, size_t *out_index) {
    if (depth >= parser->limits.maximum_depth) {
        return fail(parser, MAELYS_JSON_ERR_LIMIT, parser->offset);
    }
    size_t array;
    maelys_json_result_t result = add_token(parser, MAELYS_JSON_TYPE_ARRAY,
        parser->offset, parent, 0u, &array);
    if (result != MAELYS_JSON_OK) {
        return result;
    }
    ++parser->offset;
    skip_space(parser);
    if (!at_end(parser) && current(parser) == ']') {
        ++parser->offset;
    } else {
        for (;;) {
            size_t value;
            result = parse_value(parser, array, depth + 1u, &value);
            if (result != MAELYS_JSON_OK) {
                return result;
            }
            if (container_delimiter(parser, ']', &result)) {
                break;
            }
        }
        if (result != MAELYS_JSON_OK) {
            return result;
        }
    }
    parser->document->tokens[array].end = parser->offset;
    *out_index = array;
    return MAELYS_JSON_OK;
}

static int is_delimiter(char byte) {
    return byte == ',' || byte == '}' || byte == ']' || is_space(byte);
}

static maelys_json_result_t parse_primitive(
    parser_t *parser, size_t parent, size_t *out_index) {
    maelys_json_document_t *document = parser->document;
    size_t start = parser->offset;
    while (!at_end(parser) && !is_delimiter(current(parser))) {
        ++parser->offset;
    }
    const char *lexeme = document->bytes + start;
    size_t size = parser->offset - start;
    maelys_json_type_t type;
    unsigned char boolean_value = 0u;
    if (size == 4u && memcmp(lexeme, "true", 4u) == 0) {
        type = MAELYS_JSON_TYPE_BOOLEAN;
        boolean_value = 1u;
    } else if (size == 5u && memcmp(lexeme, "false", 5u) == 0) {
        type = MAELYS_JSON_TYPE_BOOLEAN;
    } else if (size == 4u && memcmp(lexeme, "null", 4u) == 0) {
        type = MAELYS_JSON_TYPE_NULL;
    } else if (maelys_json_number_syntax(lexeme, size)) {
        type = MAELYS_JSON_TYPE_NUMBER;
    } else {
        return fail(parser, MAELYS_JSON_ERR_SYNTAX, start);
    }
    size_t index;
    maelys_json_result_t result = add_token(parser, type, start, parent, 0u,
        &index);
    if (result != MAELYS_JSON_OK) {
        return result;
    }
    maelys_json_token_t *token = &document->tokens[index];
    token->end = parser->offset;
    token->boolean_value = boolean_value;
    if (type == MAELYS_JSON_TYPE_NUMBER) {
        char *text = arena_claim(document, size + 1u);
        memcpy(text, lexeme, size);
        text[size] = '\0';
        token->text = text;
        token->text_size = size;
        document->arena_used += size + 1u;
    }
    *out_index = index;
    return MAELYS_JSON_OK;
}

static maelys_json_result_t parse_value(
    parser_t *parser, size_t parent, size_t depth, size_t *out_index) {
    skip_space(parser);
    if (at_end(parser)) {
        return fail(parser, MAELYS_JSON_ERR_SYNTAX, parser->offset);
    }
    char byte = current(parser);
    if (byte == '{') {
        return parse_object(parser, parent, depth, out_index);
    }
    if (byte == '[') {
        return parse_array(parser, parent, depth, out_index);
    }
    if (byte == '"') {
        return parse_string(parser, parent, 0u, out_index);
    }
    return parse_primitive(parser, parent, out_index);
}

/* Lays out the children of every container contiguously so that indexed
 * access is O(1). */
static maelys_json_result_t build_children(maelys_json_document_t *document) {
    document->children = malloc(document->token_count * sizeof(*document->children));
    if (!document->children) {
        return MAELYS_JSON_ERR_MEMORY;
    }
    size_t cursor = 0u;
    for (size_t i = 0u; i < document->token_count; ++i) {
        maelys_json_token_t *token = &document->tokens[i];
        if (token->type != MAELYS_JSON_TYPE_OBJECT &&
            token->type != MAELYS_JSON_TYPE_ARRAY) {
            continue;
        }
        token->children_offset = cursor;
        for (size_t child = token->links.first_child;
             child != MAELYS_JSON_VALUE_NONE;
             child = document->tokens[child].links.next_sibling) {
            document->children[cursor++] = child;
        }
    }
    MAELYS_JSON_ASSERT(cursor + 1u == document->token_count);
    return MAELYS_JSON_OK;
}

static maelys_json_result_t allocate_document(
    const void *bytes, size_t size, const maelys_json_limits_t *limits,
    maelys_json_document_t **out_document) {
    maelys_json_document_t *document = calloc(1u, sizeof(*document));
    if (!document) {
        return MAELYS_JSON_ERR_MEMORY;
    }
    /* Every token spans at least one input byte, so `size` bounds the count
     * and the arena (decoded texts plus NULs) fits in size + tokens. */
    document->token_capacity = size < limits->maximum_tokens ? size :
        limits->maximum_tokens;
    if (!document->token_capacity) {
        document->token_capacity = 1u;
    }
    document->arena_capacity = size + document->token_capacity + 1u;
    document->bytes = malloc(size + 1u);
    document->tokens = calloc(document->token_capacity, sizeof(*document->tokens));
    document->arena = malloc(document->arena_capacity);
    if (!document->bytes || !document->tokens || !document->arena) {
        maelys_json_document_release(document);
        return MAELYS_JSON_ERR_MEMORY;
    }
    if (size) {
        memcpy(document->bytes, bytes, size);
    }
    document->bytes[size] = '\0';
    document->size = size;
    *out_document = document;
    return MAELYS_JSON_OK;
}

maelys_json_result_t maelys_json_document_parse(
    const void *bytes, size_t size, maelys_json_profile_t profile,
    const maelys_json_limits_t *requested_limits,
    maelys_json_document_t **out_document, maelys_json_error_t *out_error) {
    if (out_error) {
        *out_error = (maelys_json_error_t){0};
    }
    maelys_json_limits_t limits;
    if (!out_document || (!bytes && size) || size == SIZE_MAX ||
        !maelys_json_profile_valid(profile) ||
        maelys_json_limits_resolve(requested_limits, &limits) != MAELYS_JSON_OK) {
        maelys_json_error_set(out_error, MAELYS_JSON_ERR_ARGUMENT, NULL, 0u, 0u);
        return MAELYS_JSON_ERR_ARGUMENT;
    }
    *out_document = NULL;
    if (size > limits.maximum_bytes) {
        maelys_json_error_set(out_error, MAELYS_JSON_ERR_LIMIT, bytes, size, 0u);
        return MAELYS_JSON_ERR_LIMIT;
    }
    maelys_json_document_t *document = NULL;
    maelys_json_result_t result = allocate_document(bytes, size, &limits,
        &document);
    if (result != MAELYS_JSON_OK) {
        maelys_json_error_set(out_error, result, bytes, size, 0u);
        return result;
    }
    document->profile = profile;
    parser_t parser = {
        .document = document, .profile = profile, .limits = limits,
        .error = out_error
    };
    result = maelys_json_keyset_init(&parser.keys,
        document->token_capacity / 2u + 1u, token_key, document);
    if (result == MAELYS_JSON_OK) {
        size_t root;
        result = parse_value(&parser, MAELYS_JSON_VALUE_NONE, 0u, &root);
        skip_space(&parser);
        if (result == MAELYS_JSON_OK && parser.offset != document->size) {
            result = fail(&parser, MAELYS_JSON_ERR_SYNTAX, parser.offset);
        }
        MAELYS_JSON_ASSERT(result != MAELYS_JSON_OK || root == 0u);
    }
    maelys_json_keyset_release(&parser.keys);
    if (result == MAELYS_JSON_OK) {
        result = build_children(document);
    }
    if (result != MAELYS_JSON_OK) {
        /* Allocation failures have no position; syntax failures already
         * recorded theirs through fail(). */
        if (out_error && out_error->code == MAELYS_JSON_OK) {
            maelys_json_error_set(out_error, result, bytes, size, 0u);
        }
        maelys_json_document_release(document);
        return result;
    }
    *out_document = document;
    return MAELYS_JSON_OK;
}

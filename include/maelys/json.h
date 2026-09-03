/*
 * maelys-json: bounded JSON reader and canonical writer for Maelys contracts.
 *
 * Conventions shared by every function in this header:
 *
 * - Every fallible function returns maelys_json_result_t; MAELYS_JSON_OK is 0.
 * - MAELYS_JSON_ERR_ARGUMENT: the caller passed a NULL pointer, an invalid
 *   enumerator, an unknown flag, or text the selected profile cannot carry.
 *   Nothing was modified.
 * - MAELYS_JSON_ERR_STATE: the call is not allowed in the current state of the
 *   object (writer sequencing, finished writer, or a writer that already
 *   failed).
 * - Output parameters are written only on success unless stated otherwise.
 * - Ownership: a document owns its bytes and every view obtained from it.
 *   Views are NUL-terminated and stay valid until the document is released.
 *   Writer output is a NUL-terminated buffer the caller releases with free().
 * - Thread safety: the library has no global state and is reentrant. A parsed
 *   document is immutable and may be read from several threads concurrently.
 *   A writer must not be used from several threads at the same time.
 * - Stack usage: parsing and serialization recurse once per nesting level,
 *   bounded by MAELYS_JSON_MAXIMUM_DEPTH (roughly 50 KiB at the ceiling).
 */
#ifndef MAELYS_JSON_H
#define MAELYS_JSON_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Release version, mirrored from the VERSION file (checked by `make check`). */
#define MAELYS_JSON_VERSION_MAJOR 0
#define MAELYS_JSON_VERSION_MINOR 1
#define MAELYS_JSON_VERSION_PATCH 1
#define MAELYS_JSON_VERSION_STRING "0.1.1"

/* Incremented whenever a public type, enumerator value or symbol changes
 * incompatibly. Consumers may static_assert on it. */
#define MAELYS_JSON_ABI_VERSION 1u

/* Values used when a maelys_json_limits_t field is zero or the pointer is
 * NULL. */
#define MAELYS_JSON_DEFAULT_MAXIMUM_BYTES 65535u
#define MAELYS_JSON_DEFAULT_MAXIMUM_DEPTH 32u
#define MAELYS_JSON_DEFAULT_MAXIMUM_TOKENS 8192u

/* Absolute ceiling for maximum_depth, for both the parser and the writer. */
#define MAELYS_JSON_MAXIMUM_DEPTH 256u

/* Returned by functions that yield a maelys_json_value_t when there is none. */
#define MAELYS_JSON_VALUE_NONE SIZE_MAX

typedef struct maelys_json_document maelys_json_document_t;
typedef struct maelys_json_writer maelys_json_writer_t;

/* Handle to a value inside one document. Only meaningful with the document
 * that produced it. */
typedef size_t maelys_json_value_t;

typedef enum maelys_json_result {
    MAELYS_JSON_OK = 0,
    MAELYS_JSON_ERR_ARGUMENT = 1,
    MAELYS_JSON_ERR_MEMORY = 2,
    MAELYS_JSON_ERR_LIMIT = 3,
    MAELYS_JSON_ERR_SYNTAX = 4,
    MAELYS_JSON_ERR_UTF8 = 5,
    MAELYS_JSON_ERR_DUPLICATE_KEY = 6,
    MAELYS_JSON_ERR_TYPE = 7,
    MAELYS_JSON_ERR_RANGE = 8,
    MAELYS_JSON_ERR_STATE = 9,
    MAELYS_JSON_ERR_NOT_FOUND = 10,
    /* The value is a JSON number with a fraction or exponent. Use
     * maelys_json_value_number_text to read its lexeme. */
    MAELYS_JSON_ERR_NOT_INTEGER = 11,
    MAELYS_JSON_ERR_IO = 12
} maelys_json_result_t;

/*
 * RFC8259: strict RFC 8259 with UTF-8 validation, standard escapes, surrogate
 *   pairs. Maelys restrictions on top of the RFC: U+0000 is rejected in
 *   strings and keys (so every view is a valid C string), a byte order mark is
 *   rejected, duplicate decoded keys are rejected.
 * CONTRACT_ASCII: printable ASCII only (0x20..0x7E) in strings and keys, no
 *   escapes at all, no quotation mark or reverse solidus inside strings. It
 *   restricts decoded strings and keys to an escape-free ASCII
 *   representation. It does not make the whole document canonical:
 *   whitespace, "-0" and "0.0" are still accepted. To sign a document, parse
 *   it and require maelys_json_document_is_canonical.
 */
typedef enum maelys_json_profile {
    MAELYS_JSON_PROFILE_RFC8259 = 1,
    MAELYS_JSON_PROFILE_CONTRACT_ASCII = 2
} maelys_json_profile_t;

typedef enum maelys_json_type {
    MAELYS_JSON_TYPE_NONE = 0,
    MAELYS_JSON_TYPE_OBJECT = 1,
    MAELYS_JSON_TYPE_ARRAY = 2,
    MAELYS_JSON_TYPE_STRING = 3,
    MAELYS_JSON_TYPE_NUMBER = 4,
    MAELYS_JSON_TYPE_BOOLEAN = 5,
    MAELYS_JSON_TYPE_NULL = 6
} maelys_json_type_t;

/*
 * maximum_bytes: parser input size, or writer output size including the
 *   optional final newline, and the size of any single key or string given
 *   to the writer.
 * maximum_depth: maximum number of nested containers. `[[[]]]` has depth 3.
 *   Scalars do not count. Must not exceed MAELYS_JSON_MAXIMUM_DEPTH.
 * maximum_tokens: maximum number of values (containers, keys and scalars for
 *   the parser; nodes for the writer).
 * A zero field selects the documented default.
 */
typedef struct maelys_json_limits {
    size_t maximum_bytes;
    size_t maximum_depth;
    size_t maximum_tokens;
} maelys_json_limits_t;

/* offset is a byte offset; line and column are one-based and column counts
 * bytes, not code points. A failure detected before parsing starts (limits,
 * I/O) reports offset 0, line 1, column 1. */
typedef struct maelys_json_error {
    maelys_json_result_t code;
    size_t offset;
    size_t line;
    size_t column;
} maelys_json_error_t;

/* Borrowed bytes. data[size] is always '\0'. */
typedef struct maelys_json_view {
    const char *data;
    size_t size;
} maelys_json_view_t;

/* Writer flags, combined with bitwise OR. */
/* Append one LF after the root value. Part of Canonical JSON v1. */
#define MAELYS_JSON_WRITER_FINAL_NEWLINE 1u
/* Two-space indentation and one member per line. Presentation form, not
 * canonical. */
#define MAELYS_JSON_WRITER_INDENT 2u
/* Escape every non-ASCII code point as \uXXXX (surrogate pairs above
 * U+FFFF). Presentation form, not canonical. */
#define MAELYS_JSON_WRITER_ASCII 4u

/* Runtime version string, equal to MAELYS_JSON_VERSION_STRING of the
 * library that was linked. */
const char *maelys_json_version(void);

/* ---- Parsing ---------------------------------------------------------- */

/*
 * Parses `size` bytes into an immutable document. `bytes` may be NULL only
 * when size is 0; an empty input is a syntax error at offset 0.
 * `limits` may be NULL. `out_error` may be NULL; when given it is always
 * written and its code equals the returned value.
 * Errors: ARGUMENT, MEMORY, LIMIT (input larger than maximum_bytes, too many
 * tokens, too deep), SYNTAX, UTF8, DUPLICATE_KEY.
 */
maelys_json_result_t maelys_json_document_parse(
    const void *bytes, size_t size, maelys_json_profile_t profile,
    const maelys_json_limits_t *limits, maelys_json_document_t **out_document,
    maelys_json_error_t *out_error);

/*
 * Reads a whole file, bounded by maximum_bytes, then parses it.
 * Errors: those of maelys_json_document_parse, plus IO (open or read
 * failure) and LIMIT when the file exceeds maximum_bytes.
 */
maelys_json_result_t maelys_json_document_parse_file(
    const char *path, maelys_json_profile_t profile,
    const maelys_json_limits_t *limits, maelys_json_document_t **out_document,
    maelys_json_error_t *out_error);

/*
 * Reports whether the bytes the document was parsed from are exactly its
 * Canonical JSON v1 serialization (docs/canonical-json-v1.md), under the
 * document's own profile. `flags` is 0 or MAELYS_JSON_WRITER_FINAL_NEWLINE;
 * presentation flags are an ARGUMENT error. A document with a non-integer
 * number is reported as not canonical, not as an error. This is the check to
 * run before hashing or verifying a signed contract.
 * Errors: ARGUMENT, MEMORY.
 */
maelys_json_result_t maelys_json_document_is_canonical(
    const maelys_json_document_t *document, unsigned int flags,
    int *out_canonical);

/* Releases a document and every view obtained from it. NULL is ignored. */
void maelys_json_document_release(maelys_json_document_t *document);

/* The root value, or MAELYS_JSON_VALUE_NONE when document is NULL. */
maelys_json_value_t maelys_json_document_root(
    const maelys_json_document_t *document);

/* The type of a value, or MAELYS_JSON_TYPE_NONE for an invalid handle. */
maelys_json_type_t maelys_json_value_type(
    const maelys_json_document_t *document, maelys_json_value_t value);

/* ---- Objects and arrays (all O(1) except lookups by key, O(n)) --------- */

/* Looks up a member by its decoded key. Errors: ARGUMENT, TYPE, NOT_FOUND. */
maelys_json_result_t maelys_json_object_get(
    const maelys_json_document_t *document, maelys_json_value_t object,
    const char *key, maelys_json_value_t *out_value);

/* Same with an explicit key size; `key` may be NULL only when size is 0. */
maelys_json_result_t maelys_json_object_get_sized(
    const maelys_json_document_t *document, maelys_json_value_t object,
    const char *key, size_t key_size, maelys_json_value_t *out_value);

/* Number of members. Errors: ARGUMENT, TYPE. */
maelys_json_result_t maelys_json_object_size(
    const maelys_json_document_t *document, maelys_json_value_t object,
    size_t *out_size);

/* Member at `index` in document order. `out_key` may be NULL.
 * Errors: ARGUMENT, TYPE, RANGE. */
maelys_json_result_t maelys_json_object_member_at(
    const maelys_json_document_t *document, maelys_json_value_t object,
    size_t index, maelys_json_view_t *out_key,
    maelys_json_value_t *out_value);

/* Number of elements. Errors: ARGUMENT, TYPE. */
maelys_json_result_t maelys_json_array_size(
    const maelys_json_document_t *document, maelys_json_value_t array,
    size_t *out_size);

/* Element at `index`. Errors: ARGUMENT, TYPE, RANGE. */
maelys_json_result_t maelys_json_array_get(
    const maelys_json_document_t *document, maelys_json_value_t array,
    size_t index, maelys_json_value_t *out_value);

/* ---- Scalars ---------------------------------------------------------- */

/* Decoded UTF-8 bytes of a string. Errors: ARGUMENT, TYPE. */
maelys_json_result_t maelys_json_value_string(
    const maelys_json_document_t *document, maelys_json_value_t value,
    maelys_json_view_t *out_view);

/* Source lexeme of a number, exactly as written (for example "1.5e3").
 * Errors: ARGUMENT, TYPE. */
maelys_json_result_t maelys_json_value_number_text(
    const maelys_json_document_t *document, maelys_json_value_t value,
    maelys_json_view_t *out_view);

/* Integer readers. "-0" reads as 0. Errors: ARGUMENT, TYPE, NOT_INTEGER
 * (fraction or exponent present), RANGE (does not fit). */
maelys_json_result_t maelys_json_value_u64(
    const maelys_json_document_t *document, maelys_json_value_t value,
    uint64_t *out_number);
maelys_json_result_t maelys_json_value_i64(
    const maelys_json_document_t *document, maelys_json_value_t value,
    int64_t *out_number);

/* Errors: ARGUMENT, TYPE. */
maelys_json_result_t maelys_json_value_boolean(
    const maelys_json_document_t *document, maelys_json_value_t value,
    int *out_enabled);

/* 1 when the handle is a valid JSON null, 0 otherwise (never fails). */
int maelys_json_value_is_null(
    const maelys_json_document_t *document, maelys_json_value_t value);

/* ---- Typed member helpers (lookup + typed read in one call) ----------- */

/* Errors: those of maelys_json_object_get and of the matching reader. */
maelys_json_result_t maelys_json_object_get_string(
    const maelys_json_document_t *document, maelys_json_value_t object,
    const char *key, maelys_json_view_t *out_view);
maelys_json_result_t maelys_json_object_get_u64(
    const maelys_json_document_t *document, maelys_json_value_t object,
    const char *key, uint64_t *out_number);
maelys_json_result_t maelys_json_object_get_i64(
    const maelys_json_document_t *document, maelys_json_value_t object,
    const char *key, int64_t *out_number);
maelys_json_result_t maelys_json_object_get_boolean(
    const maelys_json_document_t *document, maelys_json_value_t object,
    const char *key, int *out_enabled);

/* ---- Writer ----------------------------------------------------------- */

/*
 * The writer builds one tree, then serializes it once. Object keys are
 * sorted in RFC 8785 (JCS) order, by UTF-16 code units of the unescaped
 * key, at serialization; arrays keep insertion order.
 *
 * Failure model: MAELYS_JSON_ERR_ARGUMENT never modifies the writer. Any
 * other error marks the writer as failed: every later call except
 * maelys_json_writer_release and maelys_json_writer_status returns
 * MAELYS_JSON_ERR_STATE. Recover by releasing the writer.
 *
 * `flags` is a bitwise OR of MAELYS_JSON_WRITER_* values, or 0. Unknown bits
 * are an ARGUMENT error. `limits` may be NULL.
 */
maelys_json_result_t maelys_json_writer_create(
    maelys_json_profile_t profile, const maelys_json_limits_t *limits,
    unsigned int flags, maelys_json_writer_t **out_writer);

/* Releases a writer. NULL is ignored. */
void maelys_json_writer_release(maelys_json_writer_t *writer);

/* MAELYS_JSON_OK while the writer is usable, the first sticky error
 * otherwise, ARGUMENT for NULL. */
maelys_json_result_t maelys_json_writer_status(
    const maelys_json_writer_t *writer);

/* Containers. Errors: LIMIT (maximum_depth or maximum_tokens), STATE. */
maelys_json_result_t maelys_json_writer_object_begin(maelys_json_writer_t *writer);
maelys_json_result_t maelys_json_writer_object_end(maelys_json_writer_t *writer);
maelys_json_result_t maelys_json_writer_array_begin(maelys_json_writer_t *writer);
maelys_json_result_t maelys_json_writer_array_end(maelys_json_writer_t *writer);

/*
 * Sets the key of the next value inside the current object. The key must be
 * valid for the profile (UTF-8 without U+0000, or printable ASCII).
 * Errors: ARGUMENT (invalid text), LIMIT (longer than maximum_bytes),
 * STATE (not inside an object, or a key is already pending),
 * DUPLICATE_KEY (same decoded bytes already used in this object).
 */
maelys_json_result_t maelys_json_writer_key(
    maelys_json_writer_t *writer, const char *key, size_t size);
maelys_json_result_t maelys_json_writer_key_cstr(
    maelys_json_writer_t *writer, const char *key);

/* Scalars. `value` must be valid for the profile.
 * Errors: ARGUMENT, LIMIT, STATE (missing key in an object, key given in an
 * array, second root value), MEMORY. */
maelys_json_result_t maelys_json_writer_string(
    maelys_json_writer_t *writer, const char *value, size_t size);
maelys_json_result_t maelys_json_writer_string_cstr(
    maelys_json_writer_t *writer, const char *value);
maelys_json_result_t maelys_json_writer_u64(
    maelys_json_writer_t *writer, uint64_t value);
maelys_json_result_t maelys_json_writer_i64(
    maelys_json_writer_t *writer, int64_t value);
maelys_json_result_t maelys_json_writer_boolean(
    maelys_json_writer_t *writer, int enabled);
maelys_json_result_t maelys_json_writer_null(maelys_json_writer_t *writer);

/*
 * Copies a value (and its subtree) from a parsed document into the writer,
 * as if the matching writer calls had been made. Numbers must be integers in
 * [-2^63, 2^64 - 1].
 * Errors: ARGUMENT (bad handle, or text the writer profile cannot carry),
 * NOT_INTEGER, RANGE, LIMIT, STATE, MEMORY. Any failure marks the writer
 * failed.
 */
maelys_json_result_t maelys_json_writer_value(
    maelys_json_writer_t *writer, const maelys_json_document_t *document,
    maelys_json_value_t value);

/*
 * Serializes the tree. With no flag other than FINAL_NEWLINE the output is
 * Maelys Canonical JSON v1 (docs/canonical-json-v1.md). The caller owns
 * *out_bytes and releases it with free(); *out_size excludes the trailing
 * NUL. The writer is finished afterwards and accepts no further values.
 * Errors: STATE (open container, pending key, no root, already finished),
 * LIMIT (output larger than maximum_bytes), MEMORY.
 */
maelys_json_result_t maelys_json_writer_finish(
    maelys_json_writer_t *writer, char **out_bytes, size_t *out_size);

/* Same, written to `stream` and flushed. Errors: those of finish, plus IO. */
maelys_json_result_t maelys_json_writer_finish_file(
    maelys_json_writer_t *writer, FILE *stream);

/* ---- Diagnostics ------------------------------------------------------ */

/* Static description of a result code. Never returns NULL. */
const char *maelys_json_result_string(maelys_json_result_t result);

/*
 * Formats an error as "line L, column C (offset O): description" into
 * `buffer`, NUL-terminated and truncated to `capacity`. Returns the length
 * of the untruncated text, like snprintf. Returns 0 for a NULL error.
 */
size_t maelys_json_error_format(
    const maelys_json_error_t *error, char *buffer, size_t capacity);

#ifdef __cplusplus
}
#endif

#endif

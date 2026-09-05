# Maelys JSON

`libmaelys-json` is the bounded JSON reader and canonical writer shared by
Maelys security contracts. C11, no dependency, opaque ABI, no global state.

- **Two profiles, one parser.** `RFC8259` is strict RFC 8259 with UTF-8
  validation and full escape handling. `CONTRACT_ASCII` restricts strings
  and keys to escape-free printable ASCII. Neither profile makes a document
  canonical by itself: `maelys_json_document_is_canonical` does.
- **Always rejected:** duplicate decoded keys, trailing bytes, invalid UTF-8,
  unpaired surrogates, U+0000, byte order marks.
- **Bounded:** byte, nesting and token limits are explicit; memory is
  proportional to the input.
- **Canonical writer:** Maelys Canonical JSON v1, that is JCS-compatible
  (RFC 8785) key ordering and string serialization with an integer-only
  numeric domain extended to exact int64/uint64. Indented and ASCII-only
  presentation forms are available as flags.

## Example

```c
#include <maelys/json.h>

maelys_json_document_t *document;
maelys_json_error_t error;
if (maelys_json_document_parse_file("config.json", MAELYS_JSON_PROFILE_RFC8259,
        NULL, &document, &error) != MAELYS_JSON_OK) {
    char message[128];
    maelys_json_error_format(&error, message, sizeof message);
    /* "line 3, column 4 (offset 12): duplicate object key" */
    /* maelys_json_error_pointer(bytes, size, &error, ...) adds the JSON
     * Pointer of the failing value, e.g. "/commands/3/payload". */
    return 1;
}
maelys_json_value_t root = maelys_json_document_root(document);
maelys_json_view_t name;
uint64_t retries;
maelys_json_object_get_string(document, root, "name", &name);   /* name.data is NUL-terminated */
maelys_json_object_get_u64(document, root, "retries", &retries);

maelys_json_writer_t *writer;
maelys_json_writer_create(MAELYS_JSON_PROFILE_RFC8259, NULL,
    MAELYS_JSON_WRITER_FINAL_NEWLINE, &writer);
maelys_json_writer_object_begin(writer);
maelys_json_writer_key_cstr(writer, "name");
maelys_json_writer_string(writer, name.data, name.size);
maelys_json_writer_key_cstr(writer, "retries");
maelys_json_writer_u64(writer, retries + 1u);
maelys_json_writer_key_cstr(writer, "original");
maelys_json_writer_value(writer, document, root);   /* copies a subtree */
maelys_json_writer_object_end(writer);

/* Or, to rewrite one member of a parsed object and keep the rest: */
static const char *const changed[] = {"retries"};
maelys_json_writer_object_begin_except(writer, document, root, changed, 1);
maelys_json_writer_key_cstr(writer, "retries");
maelys_json_writer_u64(writer, retries + 1u);
maelys_json_writer_object_end(writer);
char *bytes;
size_t size;
if (maelys_json_writer_finish(writer, &bytes, &size) == MAELYS_JSON_OK) {
    fwrite(bytes, 1, size, stdout);
    free(bytes);
}
maelys_json_writer_release(writer);
maelys_json_document_release(document);
```

Every function's contract is documented in
[`include/maelys/json.h`](include/maelys/json.h).

## Build and gates

```sh
make               # static library and pkg-config file in build/
make check         # tests with -Werror, lint, header-as-C++, policies
make asan ubsan    # sanitizer builds of the gate
make fuzz-smoke    # parser, round-trip and writer fuzzers, 15 s each
make conformance   # vendored JSONTestSuite corpus only (also part of check)
make coverage      # llvm-cov report of src/, fails under COVERAGE_MIN (90 %)
make jcs-diff      # differential test against the RFC 8785 definition (node)
make bench         # parse and canonicalize fixed inputs, ns per operation
make tidy          # clang-tidy
make cmake-check   # Release CMake build, install, find_package and pkg-config consumers
```

CMake is supported for consumers (`add_subdirectory`, target `maelys::json`);
see [`docs/integration.md`](docs/integration.md).

## Documentation

- [`docs/canonical-json-v1.md`](docs/canonical-json-v1.md): the byte format
  the writer produces.
- [`docs/security-model.md`](docs/security-model.md): what is rejected, the
  limits, and the resource bounds.
- [`docs/integration.md`](docs/integration.md): consuming the library from
  maelys-git-core and maelys-cli, version pinning, migration notes.

## Licence

MPL-2.0 (`LICENSE`), applied file by file: every source carries an
`SPDX-License-Identifier` header, enforced by `make check`. The canonical
format specification in `docs/canonical-json-v1.md` and the vectors in
`tests/vectors/` are additionally offered under CC BY 4.0 so that other
implementations can reuse them.

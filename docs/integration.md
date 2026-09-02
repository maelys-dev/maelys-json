# Integrating maelys-json

## From CMake (maelys-git-core)

maelys-git-core consumes sibling Maelys libraries with `add_subdirectory` and a
cache variable pointing at the checkout. maelys-json follows the same
convention and exports the target `maelys::json`:

```cmake
set(MAELYS_JSON_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../maelys-json"
    CACHE PATH "Path to the maelys-json source tree")
if(NOT EXISTS "${MAELYS_JSON_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "maelys-json not found; set MAELYS_JSON_SOURCE_DIR")
endif()
add_subdirectory("${MAELYS_JSON_SOURCE_DIR}"
                 "${CMAKE_CURRENT_BINARY_DIR}/maelys-json" EXCLUDE_FROM_ALL)
target_link_libraries(maelys_git_core PUBLIC maelys::json)
```

An installed copy (`cmake --install`, or `make install` for the pkg-config
route) is found with `find_package(maelys-json 0.1 CONFIG REQUIRED)` and the
same `maelys::json` target, or through pkg-config (`maelys-json`). The
package version file treats 0.x releases as compatible only within the same
minor version. `make cmake-check` runs both consumer paths against a scratch
install.

## From a Makefile (maelys-cli)

Either install the library and use pkg-config:

```sh
CFLAGS += $(shell pkg-config --cflags maelys-json)
LDLIBS += $(shell pkg-config --libs maelys-json)
```

or build it from a sibling checkout and link the archive:

```make
MAELYS_JSON_DIR ?= ../maelys-json
$(MAELYS_JSON_DIR)/build/lib/libmaelys-json.a:
	$(MAKE) -C $(MAELYS_JSON_DIR)
CPPFLAGS += -I$(MAELYS_JSON_DIR)/include
LDLIBS += $(MAELYS_JSON_DIR)/build/lib/libmaelys-json.a
```

The library's sources include each other by bare name (`#include "internal.h"`),
so compiling `src/*.c` directly into a consumer only needs `-Iinclude`.

## Pinning a version

Three locks, in increasing strength:

1. **Git tag.** Releases are tagged `v<VERSION>`. Check out the tag in the
   sibling directory, or use it as the submodule commit.
2. **Configure-time check** in the consumer:
   ```cmake
   file(STRINGS "${MAELYS_JSON_SOURCE_DIR}/VERSION" MAELYS_JSON_FOUND_VERSION LIMIT_COUNT 1)
   if(NOT MAELYS_JSON_FOUND_VERSION VERSION_EQUAL "0.1.0")
       message(FATAL_ERROR "maelys-json 0.1.0 required, found ${MAELYS_JSON_FOUND_VERSION}")
   endif()
   ```
3. **Compile-time check** in consumer code, which also catches a stale
   installed header:
   ```c
   #include <maelys/json.h>
   static_assert(MAELYS_JSON_ABI_VERSION == 1u, "maelys-json ABI mismatch");
   static_assert(MAELYS_JSON_VERSION_MAJOR == 0 && MAELYS_JSON_VERSION_MINOR == 1,
       "maelys-json 0.1 required");
   ```
   At run time, `maelys_json_version()` reports the linked library.

`MAELYS_JSON_ABI_VERSION` changes only when a public type, enumerator value or
symbol changes incompatibly.

## Migration notes

### From jansson (maelys-git-core)

| jansson | maelys-json |
|---|---|
| `json_loadb(..., JSON_REJECT_DUPLICATES)` | `maelys_json_document_parse` (duplicates always rejected) |
| `json_load_file` | `maelys_json_document_parse_file` |
| `json_object_get` + `json_is_*` + `json_*_value` | `maelys_json_object_get_string/u64/i64/boolean` |
| `json_object_foreach` | `maelys_json_object_size` + `maelys_json_object_member_at` (O(1) each) |
| `json_array_foreach` | `maelys_json_array_size` + `maelys_json_array_get` (O(1) each) |
| `json_object()`, `json_object_set_new`, `json_array_append_new`, `json_pack` | writer calls: `object_begin`, `key_cstr`, value, `object_end` |
| modifying a loaded document | parse, then `maelys_json_writer_value` for the parts kept and writer calls for the parts changed |
| `json_dumps(JSON_COMPACT \| JSON_SORT_KEYS)` | `maelys_json_writer_finish` (canonical) |
| `json_dumps(... \| JSON_ENSURE_ASCII)` | flag `MAELYS_JSON_WRITER_ASCII` (presentation form, not the bytes to sign) |
| `json_dumps(JSON_INDENT(n))` | flag `MAELYS_JSON_WRITER_INDENT` (two spaces) |
| `json_dumpf` | `maelys_json_writer_finish_file` |
| `json_real`, doubles | not supported: numbers are integers; use `maelys_json_value_number_text` to read a float lexeme |

Existing hashes or signatures computed over jansson output must be checked
against the canonical vectors before switching: jansson with
`JSON_SORT_KEYS` orders keys by `strcmp` (UTF-8 byte order), which equals
the RFC 8785 order used by maelys-json unless a key contains a character
above U+FFFF and another key contains one in U+E000..U+FFFF.
`JSON_ENSURE_ASCII` escapes with lowercase hex like `MAELYS_JSON_WRITER_ASCII`.
Integer formatting is identical.

### From the maelys-cli JSON module

The CLI's streaming writer maps one-to-one onto the writer API (`key_*`
helpers become `key_cstr` followed by the value call). `maelys_cli_json_raw`
has no equivalent by design: raw insertion would bypass validation and the
canonical order. Parse the fragment into a document and copy it with
`maelys_json_writer_value` instead. `maelys_cli_json_format` is
`maelys_json_document_parse` followed by a writer with or without
`MAELYS_JSON_WRITER_INDENT`.

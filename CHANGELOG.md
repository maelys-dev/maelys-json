# Changelog

## 0.1.3 — 2026-09-05

- Vendor the JSONTestSuite parsing corpus (MIT, 318 files) under
  `tests/conformance/` so that `make check` runs it offline; a missing corpus
  is now a failure instead of a silent skip.
- SPDX `MPL-2.0` headers on every file, enforced by `make check`; the
  canonical specification and vectors are additionally CC BY 4.0.

- Regenerate the release workflow with maelys-release 0.2.8 (the tap publish
  job no longer trips on a duplicate formula class).
- Regenerate the release workflow with maelys-release 0.2.6: the shared tap
  is tapped before bottles are built, and `workflow_dispatch` with a `tag`
  input replays the Homebrew publication of an existing tag.

## 0.1.2 — 2026-09-03

- Regenerate the release workflow with maelys-release 0.2.5. The `v0.1.1`
  tag exists but produced no release: the publish job of the socle expected
  deb and rpm packages that a library does not ship.

## 0.1.1 — 2026-09-03

- CI: initialize the string scanner's `end` so gcc's `-Wmaybe-uninitialized`
  under ASan is satisfied, and stop clang-tidy from demanding the C11 Annex K
  `_s` functions, which POSIX does not provide.
- Release through the shared maelys-release workflows: `scripts/package-release.sh
  TARGET` stages the installed library into `libmaelys-json-VERSION-TARGET.tar.gz`,
  and the Homebrew formula `libmaelys-json`, rendered from
  `packaging/homebrew/libmaelys-json.rb.in` at the released tag, builds from
  source and installs the archive, header and pkg-config file.

## 0.1.0 — 2026-09-02

Parser

- Bounded RFC 8259 profile and byte-exact Contract ASCII profile.
- Rejects duplicate decoded keys, trailing data, invalid UTF-8, unpaired
  surrogates, U+0000, and a byte order mark.
- `maximum_depth` counts nested containers only; scalars never count.
- Empty input is a syntax error at offset 0, not an argument error.
- Error positions point at the offending byte; `maelys_json_error_format`
  renders them.
- Allocation is bounded by the input size, not by `maximum_tokens`; decoded
  strings live in one arena.

Reader

- O(1) indexed access to array elements and object members.
- `maelys_json_value_number_text` exposes number lexemes;
  `MAELYS_JSON_ERR_NOT_INTEGER` distinguishes non-integers from non-numbers.
- `-0` reads as 0. `maelys_json_object_get_sized` and typed
  `maelys_json_object_get_*` helpers. `maelys_json_document_parse_file`.
- `MAELYS_JSON_TYPE_NONE` and `MAELYS_JSON_VALUE_NONE` are public.

Writer

- Maelys Canonical JSON v1: RFC 8785 key order (UTF-16 code units) and
  string serialization, exact 64-bit integers; plus INDENT and ASCII
  presentation flags and a `FILE *` output.
- Sticky failure model: any non-ARGUMENT error poisons the writer;
  `maelys_json_writer_status` reports it. Operations are atomic.
- `maelys_json_writer_value` copies a parsed subtree into the writer.
- `_cstr` variants for keys and strings.

Project

- Public header documents every contract, thread safety and ownership.
- Version macros and `maelys_json_version()`, checked against `VERSION`.
- CMake build (`maelys::json`), pkg-config, GitHub Actions CI on gcc and
  clang, clang-tidy, coverage target.
- `maelys_json_document_is_canonical` for signed-contract verification.
- Golden canonical vectors, JSONTestSuite conformance target pinned to a
  commit, three fuzz harnesses (parser, canonical round trip, writer state
  machine), Release/NDEBUG lint and an installed-package consumer check.

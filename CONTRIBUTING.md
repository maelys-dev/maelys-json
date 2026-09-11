# Contributing

## Gates

`make check` is the merge gate and runs in CI on gcc and clang, Linux and
macOS. It builds with `-Werror` and the strict warning set, runs the test
suite, compiles the public header as C++17, enforces the 1000-line file
limit, checks that `VERSION`, the header and the changelog agree, and that
the Makefile and CMake source lists match. `make asan`, `make ubsan`,
`make fuzz-smoke`, `make tidy`, `make cmake-check` (Release build with
`NDEBUG`, install, `find_package` and pkg-config consumers) are also run in
CI. The JSONTestSuite corpus is vendored under `tests/conformance/` at the
commit recorded there and runs inside `make check`; a missing corpus fails
the tests. Update it only with `tools/update-jsontestsuite.sh COMMIT`.

## Licence headers

Every `.c`, `.h`, `.cpp` and `.sh` file, the Makefile and `CMakeLists.txt`
start with `SPDX-License-Identifier: MPL-2.0` in their first two lines;
`tools/check-spdx.sh` enforces it from `make check`. The specification and
the vectors are also CC BY 4.0; keep that notice when editing them.

## Extra gates

`make jcs-diff` (needs node) compares the canonical output with the RFC 8785
definition on random integer-only documents; CI runs 2000. `make coverage`
fails under `COVERAGE_MIN` (90 %). `make bench` reports parse and
canonicalize times; compare two runs on the same machine before and after a
parser change. The `fuzz-nightly` workflow fuzzes 20 minutes per harness
every night with a corpus kept in the CI cache.

## Public API

A public function is born of a named caller or a demonstrated defect, never
of a gap perceived in the API. Name the consumer in the changelog entry. Two
functions were added in 0.1.6 without one and removed in 0.2.0 for zero
callers; that cost an ABI bump for nothing.

## Style

Formatting follows `.clang-format` (`make format`). Every control statement
uses braces. Functions stay under about 80 lines and files under 1000 lines.
Internal symbols carry `MAELYS_JSON_INTERNAL`; only what `include/maelys/json.h`
declares is public.

Error semantics: `MAELYS_JSON_ERR_ARGUMENT` means the caller passed something
invalid and nothing changed; `MAELYS_JSON_ERR_STATE` means a sequencing error.
Any non-ARGUMENT writer error is sticky. Internal invariants use
`MAELYS_JSON_ASSERT`, never error codes.

## Canonical vectors

`tests/vectors/*.canonical` pin the bytes of Maelys Canonical JSON v1. A
change to any of them is a format change: bump the version, update
`docs/canonical-json-v1.md` and the changelog, and say so in the commit.

## Releases

See [`RELEASING.md`](RELEASING.md). `VERSION` is the source of truth;
`tools/sync-version.sh` regenerates the header macros from it and
`tools/check-version.sh` fails `make check` when they drift. Consumers pin
the tag.

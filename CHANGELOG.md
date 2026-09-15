# Changelog

## Unreleased

- Release socle re-adopted at maelys-release v0.57.0 (from v0.56.0): the three
  workflow pins move and the managed agent blocks change prose only. The
  socle's CI reports its pre-0.54.0 leg names again as short alias jobs;
  `main` already requires the new names, so its protection is unchanged.
- Release socle re-adopted at maelys-release v0.56.0 (from v0.50.1): the three
  workflow pins move and the managed agent blocks change prose only. The
  socle renamed its CI legs (`check / check (linux)`, `(linux-arm64)`,
  `(macos)` instead of the runner labels), so the branch protection of
  `main` was narrowed with `protect --without-legs`, then re-derived with
  `protect --apply` after the adoption, as the socle asks.
- Release socle re-adopted at maelys-release v0.50.1 (from v0.44.0): the three
  workflow pins move and the managed agent blocks name the second managed
  dependency script; nothing here declares dependencies, channels or a
  `[commit]` rule, so no declaration changes.
- CI: the compiler matrix no longer builds its own ASan and UBSan trees; the
  socle's `sanitizers` job (`make asan-ubsan`, Linux x86_64, clang) is the
  one instrumented build per pull request, six fewer builds per run. The
  matrix keeps `make check` on gcc, clang and Apple clang. Reported by the
  fleet observer.
- Release socle re-adopted at maelys-release v0.44.0 (from v0.35.0): the three
  workflow pins move, the managed agent blocks take the corrected replay rule
  (`--ref vX.Y.Z`; a socle at fault calls for a patch release, not a replay),
  and the `[cut]` declaration moves from `packaging/release` to
  `maelys-release.conf` at the root, where socle 0.37.0 reads it.

## 0.2.0 — 2026-09-11

- **Removed** `maelys_json_value_pointer` and
  `maelys_json_document_parse_file_bytes`, added in 0.1.6 on a perceived
  gap in the API rather than on a consumer's need: neither maelys-cli nor
  maelys-git-core has a call for them (flat manifests, files read by the
  consumer's own bounded reader). `MAELYS_JSON_ABI_VERSION` is now 2; the
  CMake package treats 0.2.x as incompatible with 0.1.x. The README example
  shows the real pattern: read the bytes yourself, parse, locate a failure
  with `maelys_json_error_pointer`.
- The fuzz smoke run is delegated to the socle's `check-product.yml` job
  through `fuzz_command`; the product's own CI job is gone. The nightly
  workflow keeps the long runs.
- `packaging/release` declares `[cut] after-version sh tools/sync-version.sh`:
  the new script regenerates the header version macros from `VERSION`, so
  `maelys-release cut` can carry a release whose version lives in two files.
  `RELEASING.md` describes both the `cut` and the by-hand ceremonies, the
  three post-release checks and the replay from the tag ref.

## 0.1.6 — 2026-09-11

- `maelys_json_value_pointer`: RFC 6901 JSON Pointer of any value of a parsed
  document, for diagnostics of semantic errors (wrong type, out of range).
- `maelys_json_document_parse_file_bytes`: like `parse_file`, but hands the
  bytes read to the caller, so `maelys_json_error_pointer` can be applied to
  a file that failed to parse.
- `maelys_json_error_pointer` is documented as requiring the live input
  buffer, and its output as carrying attacker-controlled keys to escape
  before display; `maelys-json-canon` now prints the pointer as an
  ASCII-escaped JSON string instead of raw bytes (a key holding U+001B
  reached the terminal unescaped). Reported by the maelys-cli integration.
- Release socle re-adopted at maelys-release v0.35.0 (from v0.29.0); the CI
  runs once per pull-request push instead of twice.

## 0.1.5 — 2026-09-10

- Move the fuzz harnesses from `fuzz/` to `tests/fuzz/`, matching maelys-http,
  maelys-egress, maelys-oci and maelys-datalog.
- Release socle re-adopted at maelys-release v0.29.0 (from v0.15.3): the
  workflow pins move and the managed `AGENTS.md` and `CLAUDE.md` blocks gain
  their CC BY 4.0 notice and the rule that the prose lives elsewhere. The
  conventions and release-mechanism verdicts are green.
- `LICENSING.md` names the copyright holder, the licence of the installed
  agent texts and the vendored MIT JSONTestSuite corpus under
  `tests/conformance/`, and spells the archive `libmaelys-json.a`. The README
  no longer names the private documentation repository.

## 0.1.4 — 2026-09-07

- `maelys_json_writer_object_begin_except`: copy a parsed object minus the
  keys the caller replaces, leaving the object open.
- `maelys_json_error_pointer`: RFC 6901 JSON Pointer of the failing value,
  computed from the input and the error offset, without ABI change.
- Development tool `maelys-json-canon` (canonicalize, check, reformat from
  stdin), `make jcs-diff` differential test against the ECMAScript definition
  of RFC 8785 (2000 random documents in CI), `make bench`, coverage gate at
  90 % of lines, nightly fuzzing workflow with a persisted corpus.
- Release socle re-adopted at maelys-release v0.15.3 (from v0.2.8): the CI
  now runs the socle's `check-product.yml` job before the product's own jobs.

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

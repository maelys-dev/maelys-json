# Licensing

Copyright 2026 David Bromberg.

This file states what each part of maelys-json is licensed under, and names
the documents the repository engages publicly. `maelys-release check` and
`bin/maelys-platform docs` read it to tell a public engagement from prose
that migrates to `maelys-docs/maelys-json/`.

## Source code: MPL-2.0

The source code of maelys-json is available under the Mozilla Public License
2.0. The complete terms are in [`LICENSE`](LICENSE). Every maintained source,
header, test, fuzz harness, script and build file carries
`SPDX-License-Identifier: MPL-2.0`; `tools/check-spdx.sh` enforces it from
`make check`.

The MPL applies file by file. A program that links this code, statically or
otherwise, keeps its own license (section 3.3 of the MPL); only a modified
covered file must remain available in Source Code Form under MPL-2.0. This is
the intended use: `libmaelys-json.a` is linked into products that keep their
own terms.

## Installed agent texts: CC0-1.0

The managed blocks of `AGENTS.md` and `CLAUDE.md` are installed from the
`share/` texts of the Maelys distributions, which are dedicated to the public
domain under CC0-1.0. They carry no license obligation of their own.

## Redistributed material

The released artifacts redistribute none. A release ships `libmaelys-json.a`,
`include/maelys/json.h` and `maelys-json.pc`, all of them written here; the
library has no dependency beyond the C11 standard library, so nothing
third-party is embedded in the archive or linked into it.

Two sets of third-party test inputs live in the repository without reaching
any artifact:

- `tests/conformance/JSONTestSuite/` is the parsing corpus of JSONTestSuite
  by Nicolas Seriot, MIT, vendored at the commit recorded in its `COMMIT`
  file and kept byte for byte with its own `LICENSE`.
- Nothing else. The seed corpus in `tests/fuzz/corpus/` and the vectors in
  `tests/vectors/` are ours.

## Documents engaged publicly

- [`docs/canonical-json-v1.md`](docs/canonical-json-v1.md) defines the bytes
  `maelys_json_writer_finish` produces. It is a public commitment: the
  `README`, the public header and the Homebrew formula all point to it, and
  other Maelys products hash or sign those bytes. It stays in this
  repository.

  The specification is offered under MPL-2.0 OR CC-BY-4.0, so an independent
  implementation may reuse it with attribution to the Maelys project and no
  other obligation.

- `tests/vectors/` holds the reference bytes of that format, under the same
  MPL-2.0 OR CC-BY-4.0 offer, so another implementation can copy them into
  its own test suite. They are part of the commitment: changing one of those
  files changes the format.

The prose that is not engaged has already migrated to
`maelys-docs/maelys-json/`, with its history.

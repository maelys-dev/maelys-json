# Licensing

Copyright 2026 David Bromberg.

## Source code: MPL-2.0

The source of `libmaelys-json`, its headers, tools, tests and examples is
available under the Mozilla Public License 2.0. The complete terms are in
[`LICENSE`](LICENSE), and the licence applies file by file: every source
carries an `SPDX-License-Identifier` header, which `make check` enforces.

A program that links `libmaelys-json.a`, statically or otherwise, keeps its
own licence (section 3.3 of the MPL); only a modified covered file must
remain available in Source Code Form under MPL-2.0.

## The canonical format and its vectors: also CC BY 4.0

[`docs/canonical-json-v1.md`](docs/canonical-json-v1.md) specifies the bytes
the canonical writer produces, and [`tests/vectors/`](tests/vectors/) holds
the vectors that pin it. Both are offered, in addition to the MPL, under
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/), so that another
implementation can reproduce the format and check itself against the same
vectors.

This is a public engagement, and it decides where the document lives: the
specification stays in this repository, next to the code and the vectors it
governs, and is published with every release. It does not migrate to the
private documentation repository, and
[`include/maelys/json.h`](include/maelys/json.h),
[`CONTRIBUTING.md`](CONTRIBUTING.md) and
[`tests/vectors/README.md`](tests/vectors/README.md) may keep naming it.

## Documentation

The prose of this repository moved to `maelys-dev/maelys-docs`, directory
`maelys-json/`, with its history. What `docs/` keeps is what this file
engages, as the conventions of maelys-release require.

## Installed agent texts: CC-BY-4.0

The managed `maelys-release` blocks of `AGENTS.md` and `CLAUDE.md`, and
`.claude/skills/maelys-release/SKILL.md`, are installed by
`maelys-release adopt` from the maelys-release distribution. Its
`share/agents/` texts are licensed under CC-BY-4.0 with attribution to
David Bromberg since maelys-release v0.28.0. Blocks installed before that
version were copied under CC0-1.0 and that grant stands for those copies;
the notices identifying copyright, source and license arrive with the next
adoption. Retain them, and indicate your changes when sharing an
adaptation. This applies to the installed blocks, not to what this
repository writes outside them.

## Redistributed material

The released artifacts redistribute nothing: a release ships
`libmaelys-json.a`, `include/maelys/json.h` and `maelys-json.pc`, all written
here, and the library has no dependency beyond the C11 standard library. This
repository pins no other Maelys repository.

One third-party set of test inputs is vendored, and reaches no artifact:
`tests/conformance/JSONTestSuite/` is the parsing corpus of JSONTestSuite by
Nicolas Seriot, MIT, kept byte for byte at the commit recorded in its
`COMMIT` file, with its own `LICENSE`. The seed corpus of `tests/fuzz/` and
the vectors of `tests/vectors/` are ours.

# Licensing

Copyright 2026 Maelys Developers.

## Source code: MPL-2.0

The source of `libmaelys_json`, its headers, tools, tests and examples is
available under the Mozilla Public License 2.0. The complete terms are in
[`LICENSE`](LICENSE), and the licence applies file by file: every source
carries an `SPDX-License-Identifier` header, which `make check` enforces.

A program that links `libmaelys_json.a`, statically or otherwise, keeps its
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

Every other document of this repository is prose, and prose lives in
`maelys-dev/maelys-docs`, directory `maelys-json/`. The `docs/` of this
repository holds what a machine generates and what this file engages, as
the conventions of maelys-release require.

## Redistributed material

This repository vendors nothing and pins no other Maelys repository.

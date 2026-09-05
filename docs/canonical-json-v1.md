# Maelys Canonical JSON v1

<!-- SPDX-License-Identifier: MPL-2.0 OR CC-BY-4.0 -->

*Licence.* The code of this repository is MPL-2.0. This specification and the
reference vectors in `tests/vectors/` are additionally offered under the
Creative Commons Attribution 4.0 International licence (CC BY 4.0,
<https://creativecommons.org/licenses/by/4.0/>), so that an independent
implementation may copy the vectors into its own test suite with attribution
to the Maelys project and no other obligation.

This document defines the bytes emitted by `maelys_json_writer_finish` when
the writer was created with no flag, or with `MAELYS_JSON_WRITER_FINAL_NEWLINE`
only. In one sentence: JCS-compatible key ordering and string serialization,
with an integer-only numeric domain extended to exact int64/uint64.

The reference bytes live in `tests/vectors/*.canonical`. Any change to those
files is a change of this format.

## Structure

The output is exactly one JSON value. Objects are emitted with members ordered
as in RFC 8785 section 3.2.3: the unescaped keys are compared as arrays of
UTF-16 code units treated as unsigned integers; a key that is a prefix of
another sorts first, and the empty key sorts first of all. This equals Unicode
code point order (and UTF-8 byte order) except that supplementary characters
(U+10000 and above) sort before U+E000..U+FFFF, because their high surrogate
is below U+E000. Duplicate keys are rejected before serialization. Arrays
preserve insertion order. Empty containers are `{}` and `[]`. There is no
insignificant whitespace.

## Strings

Strings are valid UTF-8 without U+0000. Quotation mark, reverse solidus and
the five short control escapes are written `\"`, `\\`, `\b`, `\f`, `\n`, `\r`
and `\t`. Every other byte below U+0020 is written as lowercase `\u00xx`.
Solidus is not escaped. All other bytes, including U+007F and non-ASCII UTF-8,
are written unchanged. The Contract ASCII profile rejects quotation mark,
reverse solidus, controls and non-ASCII input instead of escaping them, so a
Contract ASCII string has exactly one byte spelling; the profile says nothing
about whitespace or numbers, so use `maelys_json_document_is_canonical` to
check a whole document.

## Numbers

Numbers are integers in [-2^63, 2^64 - 1], written in shortest base 10 with no
leading zero, no sign for zero and no exponent. The reader accepts `-0` and
reads it as 0, so `-0` canonicalizes to `0`. A document containing a fraction
or an exponent cannot be canonicalized (`MAELYS_JSON_ERR_NOT_INTEGER`).

## Literals and trailer

Booleans and null use their lowercase JSON spellings. A final LF is present if
and only if the writer was created with `MAELYS_JSON_WRITER_FINAL_NEWLINE`; it
counts toward `maximum_bytes`.

## Presentation forms (not canonical)

`MAELYS_JSON_WRITER_INDENT` emits two-space indentation, one member or element
per line and `": "` after keys, keeping the canonical order.
`MAELYS_JSON_WRITER_ASCII` escapes every code point above U+007F as lowercase
`\uxxxx`, with surrogate pairs above U+FFFF. Both parse back to the same
canonical bytes; neither is the format to sign or hash.

## Relation to RFC 8785 (JCS)

Maelys Canonical JSON v1 is byte-identical to JCS for every document whose
numbers are integers of magnitude at most 2^53, whose strings contain no
U+0000, and which is serialized without the final LF. It is deliberately not
called JCS because the two differ outside that domain:

| Aspect | RFC 8785 | Maelys v1 |
|---|---|---|
| Key order | UTF-16 code units | identical |
| String escaping | ES6 rules, lowercase hex, raw non-ASCII | identical |
| Whitespace, literals, arrays | none, lowercase, insertion order | identical |
| Duplicate keys, lone surrogates | rejected | rejected |
| Numbers | IEEE 754 doubles, ES6 formatting, exact only to 2^53 | integers only, exact over [-2^63, 2^64) |
| Fractions and exponents | serialized | rejected (`MAELYS_JSON_ERR_NOT_INTEGER`) |
| U+0000 | allowed | rejected |
| Final LF | none | optional flag |

The numeric difference is the reason for the separate name: Maelys contracts
carry 64-bit identifiers, sizes and timestamps that JCS would round.

## Failure

Exceeding a byte, nesting or node limit fails without returning partial
output and leaves the writer failed.

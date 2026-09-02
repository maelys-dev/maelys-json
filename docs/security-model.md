# Security model

## Trust boundary

The parser copies its input, decodes every string into memory it owns, and
never hands out a pointer into caller memory. Views are NUL-terminated and
valid until the document is released. Nothing in the library reads
environment variables, files (except `maelys_json_document_parse_file` with
the path the caller gives), or global state.

## What is rejected

Both profiles reject: duplicate object keys compared after decoding (so `"a"`
and `"a"` cannot coexist), trailing bytes, a byte order mark, numbers
outside the RFC 8259 grammar, control characters in strings, and U+0000 in
any form. U+0000 is a deliberate deviation from RFC 8259: it guarantees that
every view is also a valid C string, which removes a classic truncation bug
in consumers that pass `view.data` to C APIs.

`RFC8259` additionally validates UTF-8 (overlongs, surrogates encoded in
UTF-8, code points above U+10FFFF, truncated sequences) and escape syntax
(unpaired or reversed surrogates, unknown escapes).

`CONTRACT_ASCII` accepts only bytes 0x20..0x7E in strings and keys, with no
escape sequences at all. It restricts decoded strings and keys to an
escape-free ASCII representation; it does not imply that the complete
document is canonical, since whitespace, `-0` and `0.0` are still accepted.
It is not a compatibility mode. Internet JSON such as OCI manifests must use
`RFC8259`.

## Signed contracts

Signing or hashing a document requires its canonical bytes, which no parser
profile guarantees on its own. The intended sequence is:

```
parse  ->  maelys_json_document_is_canonical  ->  hash or verify the input bytes
```

`maelys_json_document_is_canonical` serializes the parsed content with the
canonical writer and compares it byte for byte with the parsed input, so a
document that passes it is exactly what the writer would have produced.
A document that fails it must be rejected, not re-canonicalized, when the
signature was computed by the sender.

Whitespace is space, tab, CR and LF only.

## Limits

Callers choose byte, depth and token limits; zero fields select the defaults
(65535 bytes, depth 32, 8192 tokens). `maximum_depth` counts nested
containers: `[[[]]]` needs depth 3, scalars never count. The absolute ceiling
is `MAELYS_JSON_MAXIMUM_DEPTH` (256) for both parser and writer; higher values
are an argument error. Recursion is bounded by that depth, roughly 50 KiB of
stack at the ceiling.

For the writer, `maximum_bytes` bounds the serialized output including the
optional final LF and any single key or string; `maximum_tokens` bounds nodes.

## Resource bounds

Parser memory is proportional to the input: at most `size` tokens are
allocated (every token spans at least one byte), decoded strings live in one
arena of `size + tokens + 1` bytes, and the duplicate-key table holds at most
`tokens / 2` entries. The 20-byte document `{"a":1,"b":[true]}` parses in
about 0.3 µs with default limits.

The duplicate-key table uses FNV-1a without a random seed. An adversary can
craft colliding keys and degrade insertion to O(n²), but n is bounded by
`maximum_tokens`, so the worst case with default limits is a few tens of
milliseconds, not a denial of service.

## Diagnostics

Errors carry the byte offset of the offending byte and one-based line and
column, where column counts bytes; failures without a position (arguments,
allocation, I/O, oversized input) report offset 0, line 1, column 1. The
error code always equals the function's return value.
`maelys_json_error_format` renders them.
No error message ever includes input bytes, so diagnostics can be logged
without leaking document content.

## Thread safety

The library is reentrant. A document is immutable after parsing and can be
read concurrently. A writer is single-threaded.

## Reporting

See SECURITY.md. Parser defects that bypass duplicate-key rejection, limits,
UTF-8 validation or canonical output are treated as security issues.

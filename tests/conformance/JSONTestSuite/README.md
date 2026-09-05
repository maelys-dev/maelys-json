# JSONTestSuite (vendored)

`test_parsing/` is the parsing corpus of JSONTestSuite by Nicolas Seriot,
<https://github.com/nst/JSONTestSuite>, MIT licence (see `LICENSE`), at the
commit recorded in `COMMIT`. Do not edit the files: run
`tools/update-jsontestsuite.sh COMMIT` to move to another upstream commit.

`make check` parses every file with the RFC 8259 profile: `y_` files must be
accepted, `n_` files rejected, `i_` files must not crash. Documented Maelys
deviations from the `y_` set (duplicate keys, U+0000) are listed in
`tests/test_conformance.c`.

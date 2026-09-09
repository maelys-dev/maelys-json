# Fuzz harnesses

libFuzzer entry points. They are never linked into the test binary and never
run by `make check`: `make fuzz-smoke` builds and runs all three for
`FUZZ_TIME` seconds each (15 by default, 30 in CI, 1200 in `fuzz-nightly`).

| Harness | What it asserts |
|---|---|
| `fuzz_parser.c` | Arbitrary bytes parse or fail cleanly under both profiles, `out_error.code` always equals the returned value, and every typed reader walks the result without a crash. |
| `fuzz_roundtrip.c` | Canonical output is a fixed point: parse, serialize, parse again and the bytes match, including through the indented and ASCII presentation forms. |
| `fuzz_writer.c` | The writer's failure model holds: after any non-ARGUMENT error every call reports STATE, and a successful finish always produces parseable JSON. |

`corpus/` holds the seed inputs and `json.dict` the libFuzzer dictionary.
Findings are written to `$(BUILD)/crash-*` and, in the nightly workflow,
uploaded as artifacts.

#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Produces a line-coverage report of src/ from a build made with
# -fprofile-instr-generate -fcoverage-mapping.
# Usage: coverage-report.sh BUILD CC [LLVM_PREFIX]
# The llvm tools must match the compiler: Apple clang needs the Xcode ones
# (xcrun), a Homebrew or distribution clang needs its own.
set -eu
build=$1
cc=$2
prefix=${3:-}
apple=0
if "$cc" --version 2>/dev/null | grep -q '^Apple'; then apple=1; fi
tool() {
    if test $apple -eq 1 && command -v xcrun >/dev/null 2>&1; then printf 'xcrun %s' "$1";
    elif test -n "$prefix" && test -x "$prefix$1"; then printf '%s' "$prefix$1";
    elif command -v "$1" >/dev/null 2>&1; then printf '%s' "$1";
    else echo "$1 not found" >&2; exit 1; fi
}
profdata=$(tool llvm-profdata)
cov=$(tool llvm-cov)
LLVM_PROFILE_FILE="$build/test.profraw" MAELYS_JSON_VECTORS=tests/vectors \
    "$build/bin/test-json" >/dev/null
$profdata merge -sparse "$build/test.profraw" -o "$build/test.profdata"
$cov report "$build/bin/test-json" -instr-profile="$build/test.profdata" src/
$cov show "$build/bin/test-json" -instr-profile="$build/test.profdata" src/ \
    -format=html -output-dir="$build/coverage" >/dev/null
echo "coverage: HTML report in $build/coverage/index.html"

#!/bin/sh
# Fetches JSONTestSuite (Nicolas Seriot, MIT) at a pinned commit for
# `make conformance`, and verifies an existing checkout is at that commit.
# Usage: fetch-jsontestsuite.sh [TARGET_DIR]
set -eu
commit=${JSONTESTSUITE_COMMIT:-1ef36fa01286573e846ac449e8683f8833c5b26a}
target=${1:-build/JSONTestSuite}
if test -d "$target/.git"; then
    found=$(git -C "$target" rev-parse HEAD)
    if test "$found" = "$commit"; then
        echo "JSONTestSuite present at $commit"
        exit 0
    fi
    echo "JSONTestSuite at $found, expected $commit; refetching" >&2
    rm -rf "$target"
fi
mkdir -p "$(dirname "$target")"
git init -q "$target"
git -C "$target" remote add origin https://github.com/nst/JSONTestSuite
git -C "$target" fetch -q --depth 1 origin "$commit"
git -C "$target" checkout -q "$commit"
test "$(git -C "$target" rev-parse HEAD)" = "$commit"
echo "JSONTestSuite fetched at $commit"
echo "run: make conformance MAELYS_JSON_TEST_SUITE=$target/test_parsing"

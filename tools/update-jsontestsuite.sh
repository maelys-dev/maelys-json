#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Replaces the vendored JSONTestSuite parsing corpus with the one at a given
# upstream commit (Nicolas Seriot, MIT), and records that commit.
# Usage: update-jsontestsuite.sh COMMIT
set -eu
commit=${1:?usage: update-jsontestsuite.sh COMMIT}
vendored=tests/conformance/JSONTestSuite
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT
git init -q "$scratch"
git -C "$scratch" remote add origin https://github.com/nst/JSONTestSuite
git -C "$scratch" fetch -q --depth 1 origin "$commit"
git -C "$scratch" checkout -q "$commit"
rm -rf "$vendored/test_parsing"
mkdir -p "$vendored/test_parsing"
cp "$scratch"/test_parsing/*.json "$vendored/test_parsing/"
cp "$scratch/LICENSE" "$vendored/LICENSE"
git -C "$scratch" rev-parse HEAD > "$vendored/COMMIT"
echo "JSONTestSuite vendored at $(cat "$vendored/COMMIT"): $(ls "$vendored/test_parsing" | wc -l | tr -d ' ') files"

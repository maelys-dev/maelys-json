#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Verifies that VERSION, the public header and the changelog agree.
set -eu
version=$(cat VERSION)
case "$version" in
    [0-9]*.[0-9]*.[0-9]*) ;;
    *) echo "VERSION must be MAJOR.MINOR.PATCH, got '$version'" >&2; exit 1 ;;
esac
major=${version%%.*}
rest=${version#*.}
minor=${rest%%.*}
patch=${rest#*.}
header=include/maelys/json.h
check() {
    grep -q "^#define $1 $2\$" "$header" || {
        echo "$header: expected '#define $1 $2'" >&2
        exit 1
    }
}
check MAELYS_JSON_VERSION_MAJOR "$major"
check MAELYS_JSON_VERSION_MINOR "$minor"
check MAELYS_JSON_VERSION_PATCH "$patch"
check MAELYS_JSON_VERSION_STRING "\"$version\""
grep -q "^## $version" CHANGELOG.md || {
    echo "CHANGELOG.md has no '## $version' section" >&2
    exit 1
}
echo "check-version: $version OK"

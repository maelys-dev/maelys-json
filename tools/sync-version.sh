#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Regenerates the version macros of include/maelys/json.h from VERSION.
# The version is materialised twice, in VERSION and in the public header;
# tools/check-version.sh fails make check when they drift, and this script
# is what closes the gap. maelys-release cut runs it between writing VERSION
# and the bump commit (maelys-release.conf, [cut] after-version), so the header
# joins that commit. Usage: sync-version.sh [--check]; --check only reports.
set -eu
header=include/maelys/json.h
version=$(sed -n '1p' VERSION)
case "$version" in
    [0-9]*.[0-9]*.[0-9]*) ;;
    *) echo "VERSION must be MAJOR.MINOR.PATCH, got '$version'" >&2; exit 1 ;;
esac
major=${version%%.*}
rest=${version#*.}
minor=${rest%%.*}
patch=${rest#*.}
case "$major$minor$patch" in
    *[!0-9]*) echo "VERSION components must be integers, got '$version'" >&2; exit 1 ;;
esac
rendered=$(sed \
    -e "s/^#define MAELYS_JSON_VERSION_MAJOR .*/#define MAELYS_JSON_VERSION_MAJOR $major/" \
    -e "s/^#define MAELYS_JSON_VERSION_MINOR .*/#define MAELYS_JSON_VERSION_MINOR $minor/" \
    -e "s/^#define MAELYS_JSON_VERSION_PATCH .*/#define MAELYS_JSON_VERSION_PATCH $patch/" \
    -e "s/^#define MAELYS_JSON_VERSION_STRING .*/#define MAELYS_JSON_VERSION_STRING \"$version\"/" \
    "$header")
for macro in MAJOR MINOR PATCH STRING; do
    printf '%s\n' "$rendered" | grep -q "^#define MAELYS_JSON_VERSION_$macro " || {
        echo "$header: no MAELYS_JSON_VERSION_$macro line to rewrite" >&2
        exit 1
    }
done
if test "$rendered" = "$(cat "$header")"; then
    echo "sync-version: $header already at $version"
    exit 0
fi
if test "${1:-}" = "--check"; then
    echo "sync-version: $header does not match VERSION $version" >&2
    exit 1
fi
printf '%s\n' "$rendered" > "$header"
echo "sync-version: $header now at $version"

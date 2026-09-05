#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Verifies that CMakeLists.txt lists exactly the library sources the
# Makefile builds, so the two build systems cannot drift.
set -eu
status=0
for source in "$@"; do
    grep -Eq "^ *$source\)?\$" CMakeLists.txt || {
        echo "CMakeLists.txt does not list $source" >&2
        status=1
    }
done
for source in $(grep -o '^ *src/[a-z_]*\.c' CMakeLists.txt | tr -d ' '); do
    case " $* " in
        *" $source "*) ;;
        *) echo "Makefile does not list $source" >&2; status=1 ;;
    esac
done
test $status -eq 0 && echo "check-cmake-sources: OK"
exit $status

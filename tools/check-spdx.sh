#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Verifies that every source, header, test, fuzz harness, script and build
# file starts with the MPL-2.0 SPDX identifier (within its first two lines).
# The MPL applies file by file, so a file without the mark loses its licence
# when copied on its own.
set -eu
status=0
files=$(find src include tests fuzz tools -type f \( -name '*.c' -o -name '*.h' \
    -o -name '*.cpp' -o -name '*.sh' \) | sort)
for f in $files Makefile CMakeLists.txt; do
    head -2 "$f" | grep -q 'SPDX-License-Identifier: MPL-2.0' || {
        echo "$f: missing 'SPDX-License-Identifier: MPL-2.0' header" >&2
        status=1
    }
done
test $status -eq 0 && echo "check-spdx: OK"
exit $status
